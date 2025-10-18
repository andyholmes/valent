/*
 * Copyright 2020 Corentin Noël <corentin.noel@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

[CCode (cheader_filename = "sysprof-capture.h")]
namespace Sysprof {
    [SimpleType]
    public struct Address : uint64 {
        public bool is_context_switch (out AddressContext context);
        public int compare (Sysprof.Address b);
    }

    [SimpleType]
    public struct CaptureAddress : uint64 {
        public int compare (Sysprof.CaptureAddress b);
    }

    namespace Collector {
        public void init ();
        public bool is_active ();
        public void allocate (Sysprof.CaptureAddress alloc_addr, int64 alloc_size, Sysprof.BacktraceFunc backtrace_func);
        public void sample (Sysprof.BacktraceFunc backtrace_func);
        public void mark (int64 time, int64 duration, string group, string mark, string? message = null);
        [PrintfFormat]
        public void mark_printf (int64 time, int64 duration, string group, string mark, string message_format, ...);
        public void mark_vprintf (int64 time, int64 duration, string group, string mark, string message_format, va_list args);
        public void log (int severity, string domain, string message);
        [PrintfFormat]
        public void log_printf (int severity, string domain, string message_format, ...);
        public uint request_counters (uint n_counters);
        public void define_counters (Sysprof.CaptureCounter[] counters);
        public void set_counters ([CCode (array_length = false)] uint[] counters_ids, Sysprof.CaptureCounterValue[] values);
    }

    [CCode (ref_function = "sysprof_capture_condition_ref", unref_function = "sysprof_capture_condition_unref", has_type_id = false)]
    public class CaptureCondition {
        public CaptureCondition.and (Sysprof.CaptureCondition left, Sysprof.CaptureCondition right);
        public CaptureCondition.or (Sysprof.CaptureCondition left, Sysprof.CaptureCondition right);
        public CaptureCondition.where_counter_in ([CCode (array_length_pos = 0.1, array_length_type = "guint")] uint[] counters);
        public CaptureCondition.where_file (string path);
        public CaptureCondition.where_pid_in ([CCode (array_length_pos = 0.1, array_length_type = "guint")] int32[] pids);
        public CaptureCondition.where_time_between (int64 begin_time, int64 end_time);
        public CaptureCondition.where_type_in ([CCode (array_length_pos = 0.1, array_length_type = "guint")] Sysprof.CaptureFrameType[] types);
        public Sysprof.CaptureCondition copy ();
        public bool match (Sysprof.CaptureFrame frame);
    }

    [CCode (ref_function = "sysprof_capture_cursor_ref", unref_function = "sysprof_capture_cursor_unref", has_type_id = false)]
    public class CaptureCursor {
        public CaptureCursor (Sysprof.CaptureReader reader);
        public unowned Sysprof.CaptureReader get_reader ();
        public void add_condition (Sysprof.CaptureCondition condition);
        public void @foreach (Sysprof.CaptureCursorCallback callback);
        public void reset ();
        public void reverse ();
    }

    [CCode (ref_function = "sysprof_capture_reader_ref", unref_function = "sysprof_capture_reader_unref", has_type_id = false)]
    public class CaptureReader {
        public CaptureReader (string filename);
        public CaptureReader.from_fd (int fd);
        public bool get_stat (out unowned Sysprof.CaptureStat st_buf);
        public bool peek_frame (Sysprof.CaptureFrame frame);
        public bool peek_type (out Sysprof.CaptureFrameType type);
        public bool read_file_fd (string path, int fd);
        public bool reset ();
        public bool save_as (string filename);
        public bool skip ();
        public bool splice (Sysprof.CaptureWriter dest);
        public int64 get_end_time ();
        public int64 get_start_time ();
        public int get_byte_order ();
        public string get_filename ();
        public string get_time ();
        [CCode (array_length = false, array_null_terminated = true)]
        public unowned string[] list_files ();
        public Sysprof.CaptureReader copy ();
        public unowned Sysprof.CaptureAllocation read_allocation ();
        public unowned Sysprof.CaptureCounterDefine read_counter_define ();
        public unowned Sysprof.CaptureCounterSet read_counter_set ();
        public unowned Sysprof.CaptureExit read_exit ();
        public unowned Sysprof.CaptureFileChunk find_file (string path);
        public unowned Sysprof.CaptureFileChunk read_file ();
        public unowned Sysprof.CaptureFork read_fork ();
        public unowned Sysprof.CaptureJitmap read_jitmap ();
        public unowned Sysprof.CaptureLog read_log ();
        public unowned Sysprof.CaptureMap read_map ();
        public unowned Sysprof.CaptureMark read_mark ();
        public unowned Sysprof.CaptureMetadata read_metadata ();
        public unowned Sysprof.CaptureProcess read_process ();
        public unowned Sysprof.CaptureSample read_sample ();
        public unowned Sysprof.CaptureTimestamp read_timestamp ();
        public void set_stat (Sysprof.CaptureStat st_buf);
    }

    [CCode (ref_function = "sysprof_capture_writer_ref", unref_function = "sysprof_capture_writer_unref", has_type_id = false)]
    public class CaptureWriter {
        public CaptureWriter (string filename, size_t buffer_size);
        public CaptureWriter.from_env (size_t buffer_size);
        public CaptureWriter.from_fd (int fd, size_t buffer_size);
        public bool add_allocation_copy (int64 time, int cpu, int32 pid, int32 tid, Sysprof.CaptureAddress alloc_addr, int64 alloc_size, Sysprof.CaptureAddress[] addrs);
        public bool add_allocation (int64 time, int cpu, int32 pid, int32 tid, Sysprof.CaptureAddress alloc_addr, int64 alloc_size, Sysprof.BacktraceFunc backtrace_func);
        public bool add_exit (int64 time, int cpu, int32 pid);
        public bool add_file_fd (int64 time, int cpu, int32 pid, string path, int fd);
        public bool add_file (int64 time, int cpu, int32 pid, string path, bool is_last, uint8[] data);
        public bool add_fork (int64 time, int cpu, int32 pid, int32 child_pid);
        public bool add_jitmap (string name);
        public bool add_log (int64 time, int cpu, int32 pid, int severity, string domain, string message);
        public bool add_map (int64 time, int cpu, int32 pid, uint64 start, uint64 end, uint64 offset, uint64 inode, string filename);
        public bool add_mark (int64 time, int cpu, int32 pid, uint64 duration, string group, string name, string message);
        public bool add_metadata (int64 time, int cpu, int32 pid, string id, string metadata, ssize_t metadata_len = -1);
        public bool add_process (int64 time, int cpu, int32 pid, string cmdline);
        public bool add_sample (int64 time, int cpu, int32 pid, int32 tid, Sysprof.CaptureAddress[] addrs);
        public bool add_timestamp (int64 time, int cpu, int32 pid);
        public bool cat (Sysprof.CaptureReader reader);
        public bool define_counters (int64 time, int cpu, int32 pid, Sysprof.CaptureCounter[] counters);
        public bool flush ();
        public bool save_as (string filename);
        public bool set_counters (int64 time, int cpu, int32 pid, [CCode (array_length = false)] uint[] counters, Sysprof.CaptureCounterValue[] values);
        public bool splice (Sysprof.CaptureWriter dest);
        public size_t get_buffer_size ();
        public Sysprof.CaptureReader create_reader ();
        public uint request_counter (uint n_counters);
        public void stat (out Sysprof.CaptureStat stat);
    }

    [CCode (has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct CaptureJitmapIter {
        void* p1;
        void* p2;
        uint u1;
        void* p3;
        void* p4;
        [CCode (cname="sysprof_capture_jitmap_iter_init")]
        public CaptureJitmapIter (Sysprof.CaptureJitmap jitmap);
        public bool next (ref Sysprof.CaptureAddress addr, out unowned string name);
    }

    [CCode (has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct CaptureStat {
        public size_t frame_count[16];
    }

    [CCode (has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct CaptureCounterValue {
        public int64 v64;
        public double vdbl;
    }

    [CCode (has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct CaptureFileHeader {
        public uint32 magic;
        public uint32 version;
        public uint32 little_endian;
        public uint32 padding;
        public char capture_time[64];
        public int64 time;
        public int64 end_time;
        public char suffix[168];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureFrame {
        public uint16 len;
        public int16  cpu;
        public int32  pid;
        public int64  time;
        public uint32 type;
        public uint32 padding1;
        public uint32 padding2;
        public uint8 data[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureMap : Sysprof.CaptureFrame {
        public uint64 start;
        public uint64 end;
        public uint64 offset;
        public uint64 inode;
        public char filename[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureJitmap : Sysprof.CaptureFrame {
        public uint32 n_jitmaps;
        public uint8 data[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureProcess : Sysprof.CaptureFrame {
        public char cmdline[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureSample : Sysprof.CaptureFrame {
        public uint32 n_addrs;
        public uint32 padding1;
        public int32 tid;
        [CCode (array_length_cname = "n_addrs")]
        public Sysprof.CaptureAddress[] addrs;
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureFork : Sysprof.CaptureFrame {
        int32 child_pid;
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureExit : Sysprof.CaptureFrame {
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureTimestamp : Sysprof.CaptureFrame {
    }

    [CCode (has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct CaptureCounter {
        public char category[32];
        public char name[32];
        public char description[52];
        public uint32 id;
        public uint32 type;
        public Sysprof.CaptureCounterValue @value;
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureCounterDefine : Sysprof.CaptureFrame {
        public uint32 n_counters;
        public uint32 padding1;
        public uint32 padding2;
        [CCode (array_length_cname = "n_counters")]
        public Sysprof.CaptureCounter[] counters;
    }

    [CCode (has_copy_function = false, has_destroy_function = false, has_type_id = false)]
    public struct CaptureCounterValues {
        public uint32 ids[8];
        public Sysprof.CaptureCounterValue values[8];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureCounterSet : Sysprof.CaptureFrame {
        public uint32 n_values;
        public uint32 padding1;
        public uint32 padding2;
        [CCode (array_length_cname = "n_values")]
        public Sysprof.CaptureCounterValues[] values;
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureMark : Sysprof.CaptureFrame {
        public int64 duration;
        public char group[24];
        public char name[40];
        public char message[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureMetadata : Sysprof.CaptureFrame {
        public char id[40];
        public char metadata[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureLog : Sysprof.CaptureFrame {
        public uint32 severity;
        public uint32 padding1;
        public uint32 padding2;
        public char domain[32];
        public char message[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureFileChunk : Sysprof.CaptureFrame {
        public uint32 is_last;
        public uint32 padding1;
        public uint32 len;
        public char path[256];
        public uint8 data[0];
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureAllocation : Sysprof.CaptureFrame {
        public Sysprof.CaptureAddress alloc_addr;
        public int64 alloc_size;
        public int32 tid;
        public uint32 n_addrs;
        public uint32 padding1;
        [CCode (array_length_cname = "n_addrs")]
        public Sysprof.CaptureAddress[] addrs;
    }

    [Compact]
    [CCode (free_function = "", has_type_id = false)]
    public class CaptureDBusMessage : Sysprof.CaptureFrame {
        public uint16 bus_type;
        public uint16 flags;
        public uint16 message_len;
        [CCode (array_length_cname = "message_len")]
        public uint8[] message;
    }

    [CCode (cprefix = "SYSPROF_CAPTURE_FRAME_", has_type_id = false)]
    public enum CaptureFrameType {
        TIMESTAMP,
        SAMPLE,
        MAP,
        PROCESS,
        FORK,
        EXIT,
        JITMAP,
        CTRDEF,
        CTRSET,
        MARK,
        METADATA,
        LOG,
        FILE_CHUNK,
        ALLOCATION,
    }

    [CCode (has_type_id = false)]
    public enum AddressContext {
        NONE,
        HYPERVISOR,
        KERNEL,
        USER,
        GUEST,
        GUEST_KERNEL,
        GUEST_USER;
        public unowned string to_string ();
    }

    [CCode (cheader_filename = "sysprof-capture.h", instance_pos = 2.9)]
    public delegate int BacktraceFunc (Sysprof.CaptureAddress[] addrs);
    [CCode (cheader_filename = "sysprof-capture.h", instance_pos = 1.9)]
    public delegate bool CaptureCursorCallback (Sysprof.CaptureFrame frame);
    [CCode (cname = "SYSPROF_CAPTURE_CURRENT_TIME")]
    public const int64 CAPTURE_CURRENT_TIME;
    [CCode (cname = "SYSPROF_CAPTURE_COUNTER_INT64")]
    public const uint32 CAPTURE_COUNTER_INT64;
    [CCode (cname = "SYSPROF_CAPTURE_COUNTER_DOUBLE")]
    public const uint32 CAPTURE_COUNTER_DOUBLE;
    [CCode (cname = "SYSPROF_CAPTURE_ADDRESS_FORMAT")]
    public const string CAPTURE_ADDRESS_FORMAT;
    [CCode (cname = "SYSPROF_CAPTURE_JITMAP_MARK")]
    public const uint64 CAPTURE_JITMAP_MARK;
    public static int memfd_create (string desc);
    [CCode (cname = "sysprof_getpagesize")]
    public static size_t get_page_size ();
}
