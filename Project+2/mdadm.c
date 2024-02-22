#include "mdadm.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool is_mounted = false;

int jbod_operation(uint32_t op, uint8_t *block);

// Helper function to print a uint32_t in binary
int mdadm_mount(void) {
  if (is_mounted) {
    return -1; // Can't mount if already mounted
  }
  uint32_t op =
      JBOD_MOUNT << 14; // Shift the mount command to the correct position
  int result = jbod_operation(
      op, NULL); // Pass NULL because block data is not required for mount
  if (result == 0) {
    is_mounted =
        true; // Set the flag to indicate that the system is now mounted
    return 1; // Success
  } else {
    return -1; // Failure
  }
}

int mdadm_unmount(void) {
  if (!is_mounted) {
    return -1; // Can't unmount if not mounted
  }
  uint32_t op = JBOD_UNMOUNT
                << 14; // Shift the unmount command to the correct position
  int result = jbod_operation(
      op, NULL); // Pass NULL because block data is not required for unmount
  if (result == 0) {
    is_mounted =
        false; // Clear the flag to indicate that the system is now unmounted
    return 1;  // Success
  } else {
    return -1; // Failure
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  if (!is_mounted) {
    printf("Not mounted, read operation failed.\n");
    return -1;
  }

  if (addr + len > JBOD_NUM_DISKS * JBOD_DISK_SIZE || len > 1024) {
    printf("Read parameters out of bounds. Addr: %u, Len: %u\n", addr, len);
    return -1;
  }

  if (len == 0) {
    // Reading zero bytes is always "successful"
    return 0;
  }

  if (!buf) {
    printf("NULL buffer provided.\n");
    return -1;
  }

  int bytes_read = 0;
  uint8_t temp_buf[JBOD_BLOCK_SIZE]; // Temporary buffer for reading blocks

  while (len > 0) {
    uint32_t disk_id = addr / JBOD_DISK_SIZE;
    uint32_t block_id = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    uint32_t block_offset = addr % JBOD_BLOCK_SIZE;
    uint32_t op = 0;

    // Seek to the correct disk
    op = (JBOD_SEEK_TO_DISK << 14) | (disk_id << 22);
    if (jbod_operation(op, NULL) != 0) {
      return -1;
    }

    // Seek to the correct block
    op = (JBOD_SEEK_TO_BLOCK << 14) | (block_id << 14);
    if (jbod_operation(op, NULL) != 0) {
      return -1;
    }

    // Calculate the number of bytes to read in this block
    uint32_t to_read = JBOD_BLOCK_SIZE - block_offset;
    to_read = (to_read > len) ? len : to_read;

    // Read the block into the temporary buffer
    op = JBOD_READ_BLOCK << 14;
    if (jbod_operation(op, temp_buf) != 0) {
      return -1;
    }

    // Copy the required data from the temporary buffer to the output buffer
    memcpy(buf, temp_buf + block_offset, to_read);

    // Update the counters and pointers
    bytes_read += to_read;
    addr += to_read;
    buf += to_read;
    len -= to_read;
  }

  return bytes_read;
}
