#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#include "jill/util/mirrored_memory.hh"


// #define report_exceptional_condition() abort ()

// typedef struct _ring_buffer_t
// {
//         char *upper_ptr;
//         char *lower_ptr;

//         std::size_t count_bytes;
//         std::size_t write_offset_bytes;
//         std::size_t read_offset_bytes;
// } ring_buffer_t;

// //Warning order should be at least 12 for Linux
// void
// ring_buffer_create (ring_buffer_t *buffer, std::size_t order)
// {
//         char *address;
//         char *lower_ptr;
//         char *upper_ptr;
//         std::size_t req_size;
//         int		shm_id		;

//         req_size = buffer->count_bytes = 1UL << order;
//         buffer->write_offset_bytes = 0;
//         buffer->read_offset_bytes = 0;

//         address = (char*)mmap (NULL,
//                         req_size + req_size,
//                         PROT_NONE,
//                         MAP_ANONYMOUS | MAP_PRIVATE,
//                         -1,
//                         0);

//         if (address == MAP_FAILED)
//                 report_exceptional_condition ();

//         mlock(address, req_size);

//         buffer->lower_ptr = lower_ptr = address;
//         buffer->upper_ptr = upper_ptr = lower_ptr + req_size;

//         //-- Unmap the portion to be used leaving requested guard pages at the ends.
//         if ( 0 > munmap( lower_ptr, req_size + req_size ) )
//                 report_exceptional_condition();

//         //-- Create a private shared memory segment.
//         if ( 0 > ( shm_id = shmget( IPC_PRIVATE, req_size, IPC_CREAT | 0700 ) ) )
//                 report_exceptional_condition();

//         if ( lower_ptr != shmat( shm_id, lower_ptr, 0 ) )
//                 report_exceptional_condition();
//         if ( upper_ptr != shmat( shm_id, upper_ptr, 0 ) )
//                 report_exceptional_condition();

//         if ( 0 > shmctl( shm_id, IPC_RMID, NULL ) )
//                 report_exceptional_condition();

// }

// //Warning order should be at least 12 for Linux
// // void
// // ring_buffer_create_1 (ring_buffer_t *buffer, std::size_t order)
// // {
// //         char path[] = "/dev/shm/ring-buffer-XXXXXX";
// //         int file_descriptor;
// //         char *address;
// //         int status;

// //         file_descriptor = mkstemp (path);
// //         if (file_descriptor < 0)
// //                 report_exceptional_condition ();

// //         status = unlink (path);
// //         if (status)
// //                 report_exceptional_condition ();

// //         buffer->count_bytes = 1UL << order;
// //         buffer->write_offset_bytes = 0;
// //         buffer->read_offset_bytes = 0;

// //         status = ftruncate (file_descriptor, buffer->count_bytes);
// //         if (status)
// //                 report_exceptional_condition ();

// //         buffer->address = (char*)mmap (NULL, buffer->count_bytes << 1, PROT_NONE,
// //                                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

// //         if (buffer->address == MAP_FAILED)
// //                 report_exceptional_condition ();

// //         address =
// //                 (char*) mmap (buffer->address, buffer->count_bytes, PROT_READ | PROT_WRITE,
// //                       MAP_FIXED | MAP_SHARED, file_descriptor, 0);

// //         if (address != buffer->address)
// //                 report_exceptional_condition ();

// //         address = (char*) mmap (buffer->address + buffer->count_bytes,
// //                         buffer->count_bytes, PROT_READ | PROT_WRITE,
// //                         MAP_FIXED | MAP_SHARED, file_descriptor, 0);

// //         if (address != buffer->address + buffer->count_bytes)
// //                 report_exceptional_condition ();

// //         status = close (file_descriptor);
// //         if (status)
// //                 report_exceptional_condition ();
// // }


// void
// ring_buffer_free (ring_buffer_t *buffer)
// {
//         // int status;
//         munlock(buffer->lower_ptr, buffer->count_bytes);
// 	shmdt( buffer->lower_ptr );
// 	shmdt( buffer->upper_ptr );
//         // status = munmap (buffer->lower_ptr, buffer->count_bytes << 1);
//         // if (status)
//         //         report_exceptional_condition ();
// }

// char *
// ring_buffer_write_address (ring_buffer_t *buffer)
// {
//         /*** void pointer arithmetic is a constraint violation. ***/
//         return buffer->lower_ptr + buffer->write_offset_bytes;
// }

// void
// ring_buffer_write_advance (ring_buffer_t *buffer,
//                            std::size_t count_bytes)
// {
//         buffer->write_offset_bytes += count_bytes;
// }

// char *
// ring_buffer_read_address (ring_buffer_t *buffer)
// {
//         return buffer->lower_ptr + buffer->read_offset_bytes;
// }

// void
// ring_buffer_read_advance (ring_buffer_t *buffer,
//                           std::size_t count_bytes)
// {
//         buffer->read_offset_bytes += count_bytes;

//         if (buffer->read_offset_bytes >= buffer->count_bytes)
//         {
//                 buffer->read_offset_bytes -= buffer->count_bytes;
//                 buffer->write_offset_bytes -= buffer->count_bytes;
//         }
// }

// std::size_t
// ring_buffer_count_bytes (ring_buffer_t *buffer)
// {
//         return buffer->write_offset_bytes - buffer->read_offset_bytes;
// }

// std::size_t
// ring_buffer_count_free_bytes (ring_buffer_t *buffer)
// {
//         return buffer->count_bytes - ring_buffer_count_bytes (buffer);
// }

// void
// ring_buffer_clear (ring_buffer_t *buffer)
// {
//         buffer->write_offset_bytes = 0;
//         buffer->read_offset_bytes = 0;
// }

unsigned short seed[3] = { 0 };

int
main(int argc, char **argv)
{
        int i;
        jill::util::mirrored_memory m;

        fprintf(stdout, "created mirrored memory with size %d bytes\n", m.size());

        char *buf = m.buffer();
        for (i = 0; i < 100; ++i) {
                buf[i] = nrand48(seed);
        }

        if (memcmp(buf, buf + m.size(), 100) != 0) {
                fprintf(stderr, "FAIL: mirrored data didn't match\n");
                return -1;
        }


        fprintf(stdout, "passed tests\n");
        return 0;
}
