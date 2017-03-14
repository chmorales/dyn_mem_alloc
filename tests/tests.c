#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "sfmm.h"

/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(sizeof(int));
  *x = 4;
  cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
  void *pointer = sf_malloc(sizeof(short));
  sf_free(pointer);
  pointer = (char*)pointer - 8;
  sf_header *sfHeader = (sf_header *) pointer;
  cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
  sf_footer *sfFooter = (sf_footer *) ((char*)pointer + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, SplinterSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini){
  void* x = sf_malloc(32);
  void* y = sf_malloc(32);
  (void)y;

  sf_free(x);

  x = sf_malloc(16);

  sf_header *sfHeader = (sf_header *)((char*)x - 8);
  cr_assert(sfHeader->splinter == 1, "Splinter bit in header is not 1!");
  cr_assert(sfHeader->splinter_size == 16, "Splinter size is not 16");

  sf_footer *sfFooter = (sf_footer *)((char*)sfHeader + (sfHeader->block_size << 4) - 8);
  cr_assert(sfFooter->splinter == 1, "Splinter bit in header is not 1!");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  memset(x, 0, 0);
  cr_assert(freelist_head->next == NULL);
  cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
  int *x = sf_malloc(4);
  int *y = sf_malloc(4);
  memset(y, 0, 0);
  sf_free(x);

  //just simply checking there are more than two things in list
  //and that they point to each other
  cr_assert(freelist_head->next != NULL);
  cr_assert(freelist_head->next->prev != NULL);
}

//#
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//#

Test(sf_memsuite, Malloc_split, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x = sf_malloc(sizeof(int));
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  sf_header* fhead = (sf_header*) freelist_head;
  cr_assert((char*)xhead + (xhead->block_size << 4) == (char*)fhead);
}

Test(sf_memsuite, Malloc_splinter, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x = sf_malloc(4064);
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  cr_assert((xhead->block_size << 4) == 4096);
  cr_assert(xhead->splinter == 1);
  cr_assert(xhead->splinter_size == 16);
  cr_assert(freelist_head == NULL);
}

Test(sf_memsuite, Malloc_padding, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x = sf_malloc(sizeof(int));
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  cr_assert(xhead->padding_size == 12);
  cr_assert(xhead->requested_size == sizeof(int));
}

Test(sf_memsuite, Malloc_new_page, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(4020);
  int* x2 = sf_malloc(48);
  sf_header* x1head = (sf_header*)((char*) x1 - SF_HEADER_SIZE);
  sf_header* x2head = (sf_header*)((char*) x2 - SF_HEADER_SIZE);
  cr_assert((char*)x1head + (x1head->block_size << 4) == (char*)x2head);
}

Test(sf_memsuite, Malloc_too_much, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(20000);
  cr_assert(errno = ENOMEM);
  cr_assert(x1 == NULL);
}

Test(sf_memsuite, Malloc_3_pages, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x = sf_malloc(12272);
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  cr_assert((xhead->block_size << 4) == 12288);
  cr_assert(freelist_head == NULL);
}

Test(sf_memsuite, Free_invalid_addr, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(sizeof(int));
  x1 = x1 + 5;
  sf_free(x1);
  cr_assert(errno = EFAULT);
}

Test(sf_memsuite, Free_freeblock, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(sizeof(int));
  memset(x1, 0,0);
  sf_free((char*)freelist_head + SF_HEADER_SIZE);
  cr_assert(errno = EINVAL);
}

Test(sf_memsuite, Free_inval_block, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(sizeof(int));
  sf_header* x1head = (sf_header*)((char*)x1 - SF_HEADER_SIZE);
  sf_footer* x1foot = (sf_footer*)((char*)x1head + (x1head->block_size << 4) - SF_FOOTER_SIZE);
  x1foot->block_size = 0;
  sf_free(x1);
  cr_assert(errno = EINVAL);
}

Test(sf_memsuite, Coalesce_right, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(sizeof(int)); // 32 byte
  sf_header* x1head = (sf_header*)((char*) x1 - SF_HEADER_SIZE);
  int* x2 = sf_malloc(sizeof(int)); // 32 byte
  sf_header* x2head = (sf_header*)((char*) x2 - SF_HEADER_SIZE);
  sf_header* fhead = (sf_header*)((char*) x2head + (x2head->block_size << 4));
  cr_assert((fhead->block_size << 4) == 4032);
  sf_free(x2);
  fhead = (sf_header*)((char*) x1head + (x1head->block_size << 4));
  cr_assert((fhead->block_size << 4) == 4064);
}

Test(sf_memsuite, Coalesce_left, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(sizeof(int));
  int* x2 = sf_malloc(sizeof(int));
  int* x3 = sf_malloc(sizeof(int));
  memset(x3, 0,0);
  sf_free(x1);
  sf_free(x2);
  sf_header* fhead = (sf_header*) freelist_head;
  cr_assert((fhead->block_size << 4) == 64);
  fhead = (sf_header*)(((sf_free_header*)fhead)->next);
  cr_assert((fhead->block_size << 4) == 4000);
}

Test(sf_memsuite, Coalesce_left_and_right, .init = sf_mem_init, .fini = sf_mem_fini){
  int* x1 = sf_malloc(sizeof(int));
  int* x2 = sf_malloc(sizeof(int));
  int* x3 = sf_malloc(sizeof(int));
  sf_free(x1);
  sf_free(x3);
  sf_header* fhead = (sf_header*) freelist_head;
  cr_assert((fhead->block_size << 4) == 32);
  fhead = (sf_header*)(((sf_free_header*)fhead)->next);
  cr_assert((fhead->block_size << 4) == 4032);
  sf_free(x2);
  fhead = (sf_header*) freelist_head;
  cr_assert((fhead->block_size << 4) == 4096);
}

Test(sf_memsuite, Realloc_shrink_split, .init = sf_mem_init, .fini = sf_mem_fini) {
  int* x = sf_malloc(100);
  *x = 100;
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  cr_assert((xhead->block_size << 4) == 128);
  sf_header* fhead = (sf_header*) freelist_head;
  cr_assert((fhead->block_size << 4) == 3968);
  int* nx = sf_realloc(x, 50);
  cr_assert(x == nx);
  cr_assert(*nx == 100);
  cr_assert((xhead->block_size << 4) == 80); // Check new blocksize
  fhead = (sf_header*) freelist_head;
  cr_assert((fhead->block_size << 4) == 4016); // Check size of freeblock
  sf_free(x);
  fhead = (sf_header*) freelist_head;
  cr_assert((fhead->block_size << 4) == 4096);
}

Test(sf_memsuite, Realloc_shrink_splinter, .init = sf_mem_init, .fini = sf_mem_fini){
  int *x = sf_malloc(48);
  *x = 100;
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  int oblock_size = (xhead->block_size << 4);
  x = sf_realloc(x, 32);
  cr_assert(*x = 100);
  cr_assert(xhead->splinter == 1);
  cr_assert(oblock_size == (xhead->block_size << 4));
  // Stats
  info* inf = malloc(sizeof(info));
  sf_info(inf);
  cr_assert(inf->allocatedBlocks == 1);
  cr_assert(inf->splinterBlocks == 1);
  cr_assert(inf->padding == 0);
  cr_assert(inf->splintering == 16);
  cr_assert(inf->coalesces == 0);
  free(inf);
}

Test(sf_memsuite, Realloc_grow_right_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
  char* x1 = sf_malloc(sizeof(char));
  char* x2 = sf_malloc(sizeof(char));
  char* x3 = sf_malloc(sizeof(char)); // peak mem = 96
  memset(x3, 0, 0);
  sf_free(x2);
  x1 = sf_realloc(x1, 32);
  // sf_snapshot(true);
  // Stats
  info* inf = malloc(sizeof(info));
  sf_info(inf);
  cr_assert(inf->allocatedBlocks == 2);
  cr_assert(inf->splinterBlocks == 1);
  cr_assert(inf->padding == 15);
  cr_assert(inf->splintering == 16);
  cr_assert(inf->coalesces == 0);
  free(inf);
}

Test(sf_memsuite, Realloc_grow_newblock_split, .init = sf_mem_init, .fini = sf_mem_fini) {
  int* x1 = sf_malloc(sizeof(int));
  *x1 = 100;
  int* x2 = sf_malloc(sizeof(int));
  memset(x2,0,0);
  int* x3 = sf_malloc(sizeof(int));
  memset(x3,0,0);
  sf_free(x2);
  cr_assert((char*)freelist_head == (char*)x2 - SF_HEADER_SIZE);
  cr_assert(freelist_head->next != NULL);
  // Realloc will need to find a new mem block
  int* nx1 = sf_realloc(x1, 65);
  cr_assert(*nx1 == 100);
  cr_assert((char*) freelist_head == (char*) x1 - SF_HEADER_SIZE);
  sf_header* fhead = (sf_header*) freelist_head;
  cr_assert(fhead->block_size << 4 == 64);
  sf_header* x3head = (sf_header*)((char*) x3 - SF_HEADER_SIZE);
  cr_assert((char*) x3head + (x3head->block_size << 4) == (char*) nx1 - SF_HEADER_SIZE);
  // Stats
  info* inf = malloc(sizeof(info));
  sf_info(inf);
  cr_assert(inf->allocatedBlocks == 2);
  cr_assert(inf->splinterBlocks == 0);
  cr_assert(inf->padding == 27);
  cr_assert(inf->splintering == 0);
  cr_assert(inf->coalesces == 1);
  free(inf);
}

Test(sf_memsuite, Realloc_grow_newblock_splinter, .init = sf_mem_init, .fini = sf_mem_fini){
  char* x1 = sf_malloc(sizeof(char));
  *x1 = 'b';
  char* x2 = sf_malloc(sizeof(char));
  memset(x2,0,0);
  char* x3 = sf_malloc(48);
  char* x4 = sf_malloc(sizeof(char));
  memset(x4,0,0);
  sf_free(x3);
  char* new_x1 = sf_realloc(x1, 32);
  sf_header* nx1head = (sf_header*)(new_x1 - SF_HEADER_SIZE);
  cr_assert((nx1head->block_size << 4) == 64);
  cr_assert((nx1head->splinter == 1));
  cr_assert(*new_x1 == 'b');
  cr_assert(x3 - SF_HEADER_SIZE == (char*) nx1head);
  cr_assert(x1 - SF_HEADER_SIZE == (char*) freelist_head);
  // Stats
  info* inf = malloc(sizeof(info));
  sf_info(inf);
  cr_assert(inf->allocatedBlocks == 3);
  cr_assert(inf->splinterBlocks == 1);
  cr_assert(inf->padding == 30);
  cr_assert(inf->splintering == 16);
  cr_assert(inf->coalesces == 0);
  free(inf);
}

Test(sf_memsuite, Realloc_grow_newpage, .init = sf_mem_init, .fini = sf_mem_fini){
  int x_val = 123242135;
  int* x = sf_malloc(4020);
  sf_header* xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  cr_assert((xhead->block_size << 4) == 4048);
  *x = x_val;
  x = sf_realloc(x, 4096);
  xhead = (sf_header*)((char*) x - SF_HEADER_SIZE);
  cr_assert((xhead->block_size << 4) == 4112);
  cr_assert(*x == x_val);

  // Stats
  info* inf = malloc(sizeof(info));
  sf_info(inf);
  cr_assert(inf->allocatedBlocks == 1);
  cr_assert(inf->splinterBlocks == 0);
  cr_assert(inf->padding == 0);
  cr_assert(inf->splintering == 0);
  cr_assert(inf->coalesces == 1);
  free(inf);
}

Test(sf_memsuite, Malloc_realloc_newpage, .init = sf_mem_init, .fini = sf_mem_fini){
  char* x = sf_malloc(4080);
  memset(x,0,0);
  cr_assert(freelist_head == NULL);
  x = sf_realloc(x, 4096);
  // print_mem();
  // sf_snapshot(true);
  sf_header* xhead = (sf_header*)(x - SF_HEADER_SIZE);
  sf_header* fhead = (sf_header*)((char*) xhead + (xhead->block_size << 4));
  cr_assert((char*)xhead + (xhead->block_size << 4) == (char*)fhead);
  cr_assert((fhead->block_size << 4) == 4080);
}

Test(sf_memsuite, Divjot_and_Chris, .init = sf_mem_init, .fini = sf_mem_fini){
  char* x1 = sf_malloc(sizeof(double));
  char* x2 = sf_malloc(400);
  char* x3 = sf_malloc(256);
  sf_free(x2);
  memset(x1,0,0);
  memset(x3,0,0);
}
