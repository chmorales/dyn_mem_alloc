/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// CONSTANTS
int PAGE_SIZE = 4096;
int ALIGNMENT = 16;

/**
 * You should store the head of your free list in this variable.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */

sf_free_header* freelist_head = NULL;
char* memhead = NULL;
int page_count = 0;

// Statistics
int allocatedBlocks = 0;
int splinterBlocks = 0;
int tot_padding = 0;
int splintering = 0;
int coalesces = 0;
int peakMemoryUtilization = 0;

int curr_memU = 0;
int curr_heapsize = 0;

// void print_mem(){
// 	sf_header* chead = (sf_header*) memhead;
// 	while((void*)chead < sf_sbrk(0)){
// 		sf_varprint((char*) chead + SF_HEADER_SIZE);
// 		chead = (sf_header*)((char*) chead + (chead->block_size << 4));
// 	}
// }

// Returns padding, aligns blocksize by align
int calcPad(int* blocksize, int align){
	int pad;
	if(*blocksize <= align*2){
		pad = align*2 - *blocksize;
		*blocksize = align*2;
		return pad;
	}
	pad = *blocksize % align;
	if(pad == 0) return 0;
	*blocksize = *blocksize + align - pad;
	return align - pad;
}

// Remove from freelist
void fl_rem(sf_free_header* node){
	if(node == freelist_head){
		freelist_head = node->next;
		if(node->next != NULL) node->next->prev = NULL;
	}else{
		if(node->prev != NULL) node->prev->next = node->next;
		else freelist_head = node->next;
		if(node->next != NULL) node->next->prev = node->prev;
	}
}

// Add to freelist, maintain freemem
void fl_add(sf_free_header* node){
	node->next = NULL, node->prev = NULL;
	if(freelist_head == NULL) freelist_head = node;
	else{
		// Find right spot in freelist
		sf_free_header* currnode = freelist_head, *prevnode = NULL;
		while(currnode != NULL && currnode < node){
			prevnode = currnode;
			currnode = currnode->next;
		}
		if(currnode == freelist_head){ // Insert at head
			node->next = currnode;
			currnode->prev = node;
			freelist_head = node;
		}else if(currnode == NULL){ // Insert at tail
			node->next = NULL;
			node->prev = prevnode;
			prevnode->next = node;
		}else{ // Insert b/w currnode and prevnode
			currnode->prev = node;
			prevnode->next = node;
			node->next = currnode;
			node->prev = prevnode;
		}
	}
}

// Merge with block to the left
// Assumes original block and left block are free and valid
sf_header* coalleft(sf_header* ohead){
	sf_footer* lfoot = (sf_footer*) ((char*) ohead - SF_FOOTER_SIZE);
	sf_header* lhead = (sf_header*)((char*) ohead - (lfoot->block_size << 4));
	fl_rem((sf_free_header*) ohead);
	fl_rem((sf_free_header*) lhead);
	lhead->block_size = ((lhead->block_size << 4) + (ohead->block_size << 4)) >> 4;
	sf_footer* nfoot = (sf_footer*)((char*) lhead + (lhead->block_size << 4) - SF_FOOTER_SIZE);
	nfoot->block_size = lhead->block_size;
	nfoot->alloc = 0;
	fl_add((sf_free_header*) lhead);
	coalesces++;
	return lhead;
}

// Merge with block to the right
// Assumes original block and right block are free and valid
sf_header* coalright(sf_header* ohead){
	sf_header* rhead = (sf_header*)((char*) ohead + (ohead->block_size << 4));
	fl_rem((sf_free_header*) rhead);
	fl_rem((sf_free_header*) ohead);
	ohead->block_size = ((ohead->block_size << 4) + (rhead->block_size << 4)) >> 4;
	sf_footer* nfoot = (sf_footer*)(((char*) ohead) + (ohead->block_size << 4) - SF_FOOTER_SIZE);
	nfoot->block_size = ohead->block_size;
	nfoot->alloc = 0;
	fl_add((sf_free_header*) ohead);
	coalesces++;
	return ohead;
}

sf_header* coalesce(sf_header* fhead){
	sf_footer* lfoot = (sf_footer*)((char*) fhead - SF_FOOTER_SIZE);
	if((char*) lfoot > memhead && !lfoot->alloc) fhead = coalleft(fhead); // If left block free, coalesce to left
	sf_header* rhead = (sf_header*)((char*) fhead + (fhead->block_size << 4));
	if((char*) rhead < (char*) sf_sbrk(0) && !rhead->alloc) fhead = coalright(fhead); // If right block free, coalesce to right
	return fhead;
}

int add_page(){
	if(page_count == 4) return -1;
	char* memptr = (char*) sf_sbrk(PAGE_SIZE);
	if(page_count == 0) memhead = memptr;
	curr_heapsize += PAGE_SIZE;
	sf_header* head = (sf_header*) memptr;
	head->block_size = (PAGE_SIZE) >> 4;
	head->alloc = 0;
	head->splinter = 0;
	head->splinter_size = 0;
	head->padding_size = 0;
	sf_footer* foot = (sf_footer*)(memptr + (head->block_size << 4) - SF_FOOTER_SIZE);
	foot->block_size = head->block_size;
	foot->alloc = 0;
	foot->splinter = 0;
	fl_add((sf_free_header*) head);
	if(page_count != 0 && !((sf_footer*)((char*) head - SF_FOOTER_SIZE))->alloc) coalleft(head);
	page_count++;
	return 1;
}

// Set requested size, padding size beforehand
sf_header* split_block(sf_header* head, int ablock_size, int fblock_size){
	head->block_size = (ablock_size >> 4);
	head->splinter = 0;
	head->splinter_size = 0;
	head->alloc = 1;
	sf_footer* foot = (sf_footer*)((char*) head + (head->block_size << 4) - SF_FOOTER_SIZE);
	foot->splinter = head->splinter;
	foot->alloc = head->alloc;
	foot->block_size = head->block_size;
	// New freeblock
	sf_header* fhead = (sf_header*)((char*) head + (head->block_size << 4));
	fhead->alloc = 0;
	fhead->block_size = fblock_size >> 4;
	fhead->splinter = 0;
	fhead->padding_size = 0;
	fhead->requested_size = 0;
	sf_footer* ffoot = (sf_footer*)((char*) fhead + (fhead->block_size << 4) - SF_FOOTER_SIZE);
	ffoot->alloc = 0;
	ffoot->block_size = fhead->block_size;
	ffoot->splinter = 0;
	fl_add((sf_free_header*) fhead);
	fhead = coalesce(fhead);
	return head;
}

// Set header feilds beforehand
sf_header* splinter_block(sf_header* head, int rem_mem){
	head->splinter_size = rem_mem;
	if(rem_mem != 0) head->splinter = 1;
	else head->splinter = 0;
	sf_footer* foot = (sf_footer*)((char*)head + (head->block_size << 4) - SF_FOOTER_SIZE);
	foot->block_size = head->block_size;
	foot->alloc = head->alloc;
	foot->splinter = head->splinter;
	return head;
}

void *sf_malloc(size_t size) {
	int block_size = SF_HEADER_SIZE + (int) size + SF_FOOTER_SIZE;
	int padding = calcPad(&block_size, ALIGNMENT);
	sf_free_header* cfhead;
	sf_header* bhead, *chead;
	int bsize, csize;
	do{
		cfhead = freelist_head;
		bhead = NULL, chead = NULL;
		bsize = 0, csize = 0;
		while(cfhead != NULL){
			chead = (sf_header*) cfhead;
			csize = chead->block_size << 4;
			if(csize >= block_size){
				if(bhead == NULL || (bsize - block_size) > (csize - block_size)){
					bhead = chead;
					bsize = bhead->block_size << 4;
				}
			}
			cfhead = cfhead->next;
		}
	}while(bhead == NULL && add_page() != -1);
	if(bhead == NULL){
		errno = ENOMEM;
		return NULL;
	}
	// Assign header values
	bhead->alloc = 1;
	bhead->requested_size = size;
	bhead->padding_size = padding;
	// Remove from freelist
	fl_rem((sf_free_header*) bhead);
	int rem_mem = bsize - block_size;
	if((rem_mem) < 32) bhead = splinter_block(bhead, rem_mem);
	else bhead = split_block(bhead, block_size, rem_mem);
	// Update stats
	allocatedBlocks++;
	tot_padding += bhead->padding_size;
	if(bhead->splinter) splinterBlocks++;
	splintering += bhead->splinter_size;
	// curr_memU += bhead->block_size << 4;
	curr_memU += size;
	if(curr_memU > peakMemoryUtilization) peakMemoryUtilization = curr_memU;
	// Return payload pointer
	char* plptr = (char*) bhead + SF_HEADER_SIZE;
	return (void*) plptr;
}

void *sf_realloc(void *ptr, size_t size) {
	if(size == 0){
	 	sf_free(ptr);
	 	return NULL;
	}
	sf_header* ohead = (sf_header*) ((char*)ptr - SF_HEADER_SIZE);
	// For later use by stats
	int orig_padding = ohead->padding_size;
	int orig_splinter_size = ohead->splinter_size;
	int orig_req_size = ohead->requested_size;
	// Update header
	int oblock_size = ohead->block_size << 4;
	int nblock_size = SF_HEADER_SIZE + size + SF_FOOTER_SIZE;
	int padding = calcPad(&nblock_size, ALIGNMENT);
	if(oblock_size == nblock_size){
		ohead->splinter_size = 0;
		ohead->splinter = 0;
		sf_footer* ofoot = (sf_footer*)((char*) ohead + (ohead->block_size << 4) - SF_FOOTER_SIZE);
		ofoot->splinter = 0;
		return ptr;
	}
	if(oblock_size > nblock_size){ // SHRINKING CASE
		ohead->padding_size = padding;
		ohead->requested_size = size;
		int rem_mem = oblock_size - nblock_size;
		if(rem_mem < 32) ohead = splinter_block(ohead, rem_mem);
		else ohead = split_block(ohead, nblock_size, rem_mem);
		// Update statistics
		tot_padding -= orig_padding;
		splintering -= orig_splinter_size;
		if(orig_splinter_size) splinterBlocks--;
		// curr_memU -= oblock_size;
		curr_memU -= orig_req_size;
		// New stats
		tot_padding += ohead->padding_size;
		splintering += ohead->splinter_size;
		if(ohead->splinter) splinterBlocks++;
		curr_memU += ohead->requested_size;
		if(curr_memU > peakMemoryUtilization) peakMemoryUtilization = curr_memU;
		// Return new pointer
		char* plptr = (char*) ohead + SF_HEADER_SIZE;
		return (void*) plptr;
	}else{ // GROWING CASE
		sf_header* nblock = NULL;
		sf_header* rhead = NULL;
		int coalrflg = 0;
		// For looping through freelist
		sf_free_header* curr_free;
		sf_header* best_head, *curr_head;
		int best_block_size, curr_block_size;
		do{
			rhead = (sf_header*)((char*) ohead + (ohead->block_size << 4));
			int cblock_size = (ohead->block_size << 4) + (rhead->block_size << 4);
			if((void*) rhead < sf_sbrk(0) && !rhead->alloc && cblock_size >= nblock_size) { // Combine right case
				coalrflg = 1;
				fl_rem((sf_free_header*) rhead);
				ohead->alloc = 1;
				ohead->padding_size = padding;
				ohead->block_size = nblock_size >> 4;
				ohead->requested_size = size;
				int rem_mem = cblock_size - nblock_size;
				if(rem_mem < 32){
					ohead->block_size = cblock_size >> 4;
					nblock = splinter_block(ohead, rem_mem);
				}else{
					// ohead->block_size = nblock_size >> 4;
					nblock = split_block(ohead, nblock_size, rem_mem);
				}
			}else{ // Search freelist case
				coalrflg = 0;
				curr_free = freelist_head;
				best_head = NULL;
				while(curr_free != NULL){
					curr_head = (sf_header*) curr_free;
					curr_block_size = curr_head->block_size << 4;
					if(curr_block_size >= nblock_size){
						if(best_head == NULL || (best_block_size - nblock_size) > (curr_block_size - nblock_size)){
							best_head = curr_head;
							best_block_size = curr_block_size;
						}
					}
					curr_free = curr_free->next;
				}
				if(best_head != NULL) {
					fl_rem((sf_free_header*) best_head);
					best_head->alloc = 1;
					best_head->requested_size = size;
					best_head->padding_size = padding;
					int rem_mem = best_block_size - nblock_size;
					if(rem_mem < 32) best_head = splinter_block(best_head, rem_mem);
					else best_head = split_block(best_head, nblock_size, rem_mem);
					char* new_plptr = (char*) best_head + SF_HEADER_SIZE;
					new_plptr = memcpy(new_plptr, ptr, size);
					sf_free(ptr);
					allocatedBlocks++; // Because of the above free
					nblock = best_head;
				}
			}
		}while(nblock == NULL && add_page() != -1);
		if(nblock == NULL){
			errno = ENOMEM;
			return NULL;
		}
		if(coalrflg){
			// Remove old block from stats
			tot_padding -= orig_padding;
			splintering -= orig_splinter_size;
			if(orig_splinter_size) splinterBlocks--;
			curr_memU -= orig_req_size;
		}
		// Update new stats
		tot_padding += nblock->padding_size;
		splintering += nblock->splinter_size;
		splinterBlocks += nblock->splinter;
		// curr_memU += nblock->block_size << 4;
		curr_memU += size;
		if(curr_memU > peakMemoryUtilization) peakMemoryUtilization = curr_memU;
		char* plptr = (char*) nblock + SF_HEADER_SIZE;
		return (void*) plptr;
	}
}

void sf_free(void* ptr) {
	if ((uintptr_t)ptr % ALIGNMENT != 0){
		errno = EFAULT;
		return;
	}
	// Update header and footer
	sf_header* head = (sf_header*)((char*) ptr - SF_HEADER_SIZE);
	sf_footer* foot = (sf_footer*)((char*) head + (head->block_size << 4) - SF_FOOTER_SIZE);
	if(head->block_size != foot->block_size || head->splinter != foot->splinter || head->alloc == 0 || foot->alloc == 0){
		errno = EINVAL;
		return;
	}
	// Stats
	allocatedBlocks--;
	curr_memU -= head->requested_size;
	tot_padding -= head->padding_size;
	splintering -= head->splinter_size;
	if(head->splinter) splinterBlocks--;
	// Feilds
	head->alloc = 0;
	head->padding_size = 0;
	head->requested_size = 0;
	head->splinter = 0;
	head->splinter_size = 0;
	foot->alloc = 0;
	// Add to freelist
	fl_add((sf_free_header*) head);
	// COALESCE
	head = coalesce(head);
}

int sf_info(info* ptr) {
	ptr->allocatedBlocks = allocatedBlocks;
	ptr->splinterBlocks = splinterBlocks;
	ptr->padding = tot_padding;
	ptr->splintering = splintering;
	ptr->coalesces = coalesces;
	double peak_mem_util = (peakMemoryUtilization / curr_heapsize);
	ptr->peakMemoryUtilization = peak_mem_util;
	return 0;
}
