/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* syscall_pt_trace.cpp: module for recording kernel PT traces for every syscall. */

#include "syscall_pt_trace.h"

#include <stdint.h>

#include <string>

#include "dr_api.h"
#include "trace_entry.h"
#include "utils.h"
#include "drmemtrace.h"
#include "drpttracer.h"
/* For SYS_exit,SYS_exit_group. */
#include "../../../core/unix/include/syscall_linux_x86.h"

namespace dynamorio {
namespace drmemtrace {

#ifndef BUILD_PT_TRACER
#    error "This module requires the drpttracer extension."
#endif

#ifndef OUTFILE_SUFFIX_PT
#    define OUTFILE_SUFFIX_PT "raw.pt"
#endif
#define RING_BUFFER_SIZE_SHIFT 8

syscall_pt_trace_t::syscall_pt_trace_t()
    : open_file_func_(nullptr)
    , write_file_func_(nullptr)
    , close_file_func_(nullptr)
    , pttracer_handle_ { GLOBAL_DCONTEXT, nullptr }
    , pttracer_output_buffer_ { GLOBAL_DCONTEXT, nullptr }
    , traced_syscall_idx_(0)
    , cur_recording_sysnum_(-1)
    , is_dumping_metadata_(false)
    , drcontext_(nullptr)
    , output_file_(INVALID_FILE)
{
}

syscall_pt_trace_t::~syscall_pt_trace_t()
{
    if (output_file_ != INVALID_FILE) {
        close_file_func_(output_file_);
        output_file_ = INVALID_FILE;
    }
}

bool
syscall_pt_trace_t::init(void *drcontext, char *pt_dir_name,
                         drmemtrace_open_file_func_t open_file_func,
                         drmemtrace_write_file_func_t write_file_func,
                         drmemtrace_close_file_func_t close_file_func)
{
    if (is_initialized_) {
        ASSERT(false, "syscall_pt_trace_t is already initialized");
        return false;
    }
    drcontext_ = drcontext;
    open_file_func_ = open_file_func;
    write_file_func_ = write_file_func;
    close_file_func_ = close_file_func;
    pttracer_handle_ = { drcontext, nullptr };
    pttracer_output_buffer_ = { drcontext_, nullptr };
    std::string output_file_name(pt_dir_name);
    output_file_name +=
        "/" + std::to_string(dr_get_thread_id(drcontext_)) + "." + OUTFILE_SUFFIX_PT;
    output_file_ = open_file_func_(output_file_name.c_str(), DR_FILE_WRITE_REQUIRE_NEW);

    /* Create a buffer to store the data generated by drpttracer. For syscall traces, only
     * the PT data is dumped, and the sideband data is not included.
     */
    if (drpttracer_create_output(drcontext_, RING_BUFFER_SIZE_SHIFT, 0,
                                 &pttracer_output_buffer_.data) != DRPTTRACER_SUCCESS) {
        return false;
    }

    is_initialized_ = true;
    return true;
}

bool
syscall_pt_trace_t::start_syscall_pt_trace(IN int sysnum)
{
    ASSERT(is_initialized_, "syscall_pt_trace_t is not initialized");
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");

    if (drpttracer_create_handle(drcontext_, DRPTTRACER_TRACING_ONLY_KERNEL,
                                 RING_BUFFER_SIZE_SHIFT, RING_BUFFER_SIZE_SHIFT,
                                 &pttracer_handle_.handle) != DRPTTRACER_SUCCESS) {
        return false;
    }

    /* All syscalls within a single thread share the same pttracer configuration, and
     * thus, the same pt_metadata. Metadata is dumped at the beginning of the output
     * file and occurs only once.
     */
    if (!is_dumping_metadata_) {
        pt_metadata_t pt_metadata;
        if (drpttracer_get_pt_metadata(pttracer_handle_.handle, &pt_metadata)) {
            return false;
        }
        if (!metadata_dump(pt_metadata)) {
            return false;
        }
        is_dumping_metadata_ = true;
    }

    /* Start tracing the current syscall. */
    if (drpttracer_start_tracing(drcontext_, pttracer_handle_.handle) !=
        DRPTTRACER_SUCCESS) {
        return false;
    }
    cur_recording_sysnum_ = sysnum;
    return true;
}

bool
syscall_pt_trace_t::stop_syscall_pt_trace()
{
    ASSERT(is_initialized_, "syscall_pt_trace_t is not initialized");
    ASSERT(drcontext_ != nullptr, "drcontext_ is nullptr");
    ASSERT(pttracer_handle_.handle != nullptr, "pttracer_handle_.handle is nullptr");
    ASSERT(pttracer_output_buffer_.data != nullptr,
           "pttracer_output_buffer_.data is nullptr");
    ASSERT(output_file_ != INVALID_FILE, "output_file_ is INVALID_FILE");

    if (drpttracer_stop_tracing(drcontext_, pttracer_handle_.handle,
                                pttracer_output_buffer_.data) != DRPTTRACER_SUCCESS) {
        return false;
    }

    if (!trace_data_dump(pttracer_output_buffer_)) {
        return false;
    }

    traced_syscall_idx_++;
    cur_recording_sysnum_ = -1;

    /* Reset the pttracer handle for next syscall.
     * TODO i#5505: To reduce the overhead caused by pttracer initialization, we need to
     * share the same pttracer handle for all syscalls per thread. And also, we need to
     * improve the libpt2ir to support streaming decoding.
     */
    pttracer_handle_.reset();
    return true;
}

bool
syscall_pt_trace_t::metadata_dump(pt_metadata_t metadata)
{
    ASSERT(output_file_ != INVALID_FILE, "output_file_ is INVALID_FILE");
    if (output_file_ == INVALID_FILE) {
        return false;
    }

    /* Initialize the header of output buffer. */
    syscall_pt_entry_t pdb_header[PT_METADATA_PDB_HEADER_ENTRY_NUM];
    pdb_header[PDB_HEADER_PID_IDX].pid.type = SYSCALL_PT_ENTRY_TYPE_PID;
    pdb_header[PDB_HEADER_PID_IDX].pid.pid = dr_get_process_id_from_drcontext(drcontext_);
    pdb_header[PDB_HEADER_TID_IDX].tid.type = SYSCALL_PT_ENTRY_TYPE_THREAD_ID;
    pdb_header[PDB_HEADER_TID_IDX].tid.tid = dr_get_thread_id(drcontext_);
    pdb_header[PDB_HEADER_DATA_BOUNDARY_IDX].pt_metadata_boundary.data_size =
        sizeof(pt_metadata_t);
    pdb_header[PDB_HEADER_DATA_BOUNDARY_IDX].pt_metadata_boundary.type =
        SYSCALL_PT_ENTRY_TYPE_PT_METADATA_BOUNDARY;

    /* Write the buffer header to the output file */
    if (write_file_func_(output_file_, pdb_header, PT_METADATA_PDB_HEADER_SIZE) == 0) {
        ASSERT(false, "Failed to write the metadata's header to the output file");
        return false;
    }

    /* Write the pt_metadata to the output file */
    if (write_file_func_(output_file_, &metadata, sizeof(pt_metadata_t)) == 0) {
        ASSERT(false, "Failed to write the metadata to the output file");
        return false;
    }

    return true;
}

bool
syscall_pt_trace_t::trace_data_dump(drpttracer_output_autoclean_t &output)
{
    ASSERT(output_file_ != INVALID_FILE, "output_file_ is INVALID_FILE");
    if (output_file_ == INVALID_FILE) {
        return false;
    }

    drpttracer_output_t *data = output.data;
    ASSERT(data != nullptr, "output.data is nullptr");
    ASSERT(data->pt_buffer != nullptr, "pt_buffer is nullptr");
    ASSERT(data->pt_size > 0, "pt_size is 0");
    if (data == nullptr || data->pt_buffer == nullptr || data->pt_size == 0) {
        return false;
    }

    /* Initialize the header of output buffer. */
    syscall_pt_entry_t pdb_header[PT_DATA_PDB_HEADER_ENTRY_NUM];
    pdb_header[PDB_HEADER_PID_IDX].pid.type = SYSCALL_PT_ENTRY_TYPE_PID;
    pdb_header[PDB_HEADER_PID_IDX].pid.pid = dr_get_process_id_from_drcontext(drcontext_);
    pdb_header[PDB_HEADER_TID_IDX].tid.type = SYSCALL_PT_ENTRY_TYPE_THREAD_ID;
    pdb_header[PDB_HEADER_TID_IDX].tid.tid = dr_get_thread_id(drcontext_);
    pdb_header[PDB_HEADER_DATA_BOUNDARY_IDX].pt_data_boundary.type =
        SYSCALL_PT_ENTRY_TYPE_PT_DATA_BOUNDARY;

    /* Initialize the sysnum. */
    pdb_header[PDB_HEADER_SYSNUM_IDX].sysnum.type = SYSCALL_PT_ENTRY_TYPE_SYSNUM;
    pdb_header[PDB_HEADER_SYSNUM_IDX].sysnum.sysnum = cur_recording_sysnum_;

    /* Initialize the syscall id. */
    pdb_header[PDB_HEADER_SYSCALL_IDX_IDX].syscall_idx.type =
        SYSCALL_PT_ENTRY_TYPE_SYSCALL_IDX;
    pdb_header[PDB_HEADER_SYSCALL_IDX_IDX].syscall_idx.idx = traced_syscall_idx_;

    /* Initialize the parameter of current recorded syscall.
     * TODO i#5505: dynamorio doesn't provide a function to get syscall's
     * parameter number. So currently we can't get and dump any syscall's
     * parameters. And we dump the parameter number with a fixed value 0. We
     * should fix this issue by implementing a new function that can get syscall's
     * parameter number.
     */
    pdb_header[PDB_HEADER_NUM_ARGS_IDX].syscall_args_num.type =
        SYSCALL_PT_ENTRY_TYPE_SYSCALL_ARGS_NUM;
    pdb_header[PDB_HEADER_NUM_ARGS_IDX].syscall_args_num.args_num = 0;

    /* Initialize the size of the PDB data. */
    pdb_header[PDB_HEADER_DATA_BOUNDARY_IDX].pt_data_boundary.data_size =
        SYSCALL_METADATA_SIZE +
        pdb_header[PDB_HEADER_NUM_ARGS_IDX].syscall_args_num.args_num * sizeof(uint64_t) +
        data->pt_size;

    /* Write the buffer header to the output file */
    if (write_file_func_(output_file_, pdb_header, PT_DATA_PDB_HEADER_SIZE) == 0) {
        ASSERT(false, "Failed to write the trace data's header to the output file");
        return false;
    }

    /* Write the syscall's PT data to the output file */
    if (write_file_func_(output_file_, data->pt_buffer, data->pt_size) == 0) {
        ASSERT(false, "Failed to write the trace data to the output file");
        return false;
    }
    return true;
}

bool
syscall_pt_trace_t::is_syscall_pt_trace_enabled(IN int sysnum)
{
    /* The following syscall's post syscall callback can't be triggered. So we don't
     * support to recording the kernel PT of them.
     */
    if (sysnum == SYS_exit || sysnum == SYS_exit_group || sysnum == SYS_execve ||
        sysnum == SYS_rt_sigreturn) {
        return false;
    }
    return true;
}

} // namespace drmemtrace
} // namespace dynamorio