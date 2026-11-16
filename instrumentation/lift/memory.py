"""Limit the number of harness runs by the free memory of the PC.

A harness run keeps about 2.3 GB (the snapshot, the copy of the data of the image and the heap of the
original). Windows pays for memory above the RAM with the page file on the system disk, and it does not
give that space back until a restart. Fewer runs at the same time keep the page file small.

Library: safe_jobs(requested, per_job_mb=2300, keep_mb=2500) -> number of runs (at least 1).
"""
import ctypes
import os


class _MemoryStatus(ctypes.Structure):
    _fields_ = [('length', ctypes.c_ulong), ('load', ctypes.c_ulong),
                ('total_phys', ctypes.c_ulonglong), ('avail_phys', ctypes.c_ulonglong),
                ('total_page', ctypes.c_ulonglong), ('avail_page', ctypes.c_ulonglong),
                ('total_virtual', ctypes.c_ulonglong), ('avail_virtual', ctypes.c_ulonglong),
                ('avail_extended', ctypes.c_ulonglong)]


def free_physical_mb():
    """Free physical memory in MB, or None when the system does not tell."""
    if os.name != 'nt':
        return None
    status = _MemoryStatus()
    status.length = ctypes.sizeof(status)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        return None
    return status.avail_phys // (1024 * 1024)


def safe_jobs(requested, per_job_mb=2300, keep_mb=2500):
    """The number of runs that fit in the free memory, and keep_mb free for the rest of the PC."""
    free = free_physical_mb()
    if free is None:
        return max(1, requested)
    return max(1, min(requested, (free - keep_mb) // per_job_mb))


if __name__ == '__main__':
    print(f'free {free_physical_mb()} MB, safe runs {safe_jobs(4)} of 4')
