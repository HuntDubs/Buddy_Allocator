  /**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	/* TODO: DECLARE NECESSARY MEMBER VARIABLES */
	int blockOrder;
	int free;
	int index;
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
char g_memory[1<<MAX_ORDER];

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

void* allocAddr;

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/
void* findFree(int curIndex, int targetIndex){
	if(curIndex == targetIndex && !list_empty(&free_area[curIndex])){
		page_t* left = list_entry(free_area[curIndex].prev, page_t, list);
		struct list_head* counter;
		list_for_each(counter, &free_area[curIndex]){
			if(  (list_entry(counter, page_t, list)->index) < (left->index) ){
				left = list_entry(counter, page_t, list);
			}
		}
		list_del(&(left->list));
		g_pages[left->index].free = 0;
		allocAddr = PAGE_TO_ADDR(left->index);
		// return PAGE_TO_ADDR(left->index);
	} else if(curIndex > targetIndex && !list_empty(&free_area[curIndex])){
		int leftPageIndex = -1;
		int rightPageIndex = -1;
		page_t* left = list_entry(free_area[curIndex].prev, page_t, list);
		struct list_head* counter;
		list_for_each(counter, &free_area[curIndex]){
			if(  (list_entry(counter, page_t, list)->index) < (left->index) ){
				left = list_entry(counter, page_t, list);
			}
		}
		leftPageIndex = left->index;
		rightPageIndex = ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(leftPageIndex), ((left->blockOrder)-1)));

		list_del(&(left->list));
		g_pages[leftPageIndex].blockOrder = curIndex-1;
		g_pages[rightPageIndex].blockOrder = curIndex-1;
		list_add(&g_pages[leftPageIndex].list, &free_area[curIndex-1]);
		list_add(&g_pages[rightPageIndex].list, &free_area[curIndex-1]);
		findFree(curIndex-1, targetIndex);
	} else {
			findFree(curIndex+1, targetIndex);
	}
	return 0;
}


/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;
	for (i = 0; i < n_pages; i++) {
		/* TODO: INITIALIZE PAGE STRUCTURES */
		INIT_LIST_HEAD(&g_pages[i].list);
		g_pages[i].blockOrder = -1;
		g_pages[i].free = 1;
		g_pages[i].index = i;
	}
	g_pages[0].blockOrder = MAX_ORDER;

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
	//shift bit left until we hit bit just bigger than size
	int i;
	for(i = 0; (1 << i) < size; i++){}
	int index = i;
	findFree(MIN_ORDER, index);
	return allocAddr;
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	page_t* pageToFree = &g_pages[ADDR_TO_PAGE(addr)];
	int order = pageToFree->blockOrder;

	int i;

	for(i = order; i < MAX_ORDER; i++){
		page_t* buddy = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(pageToFree->index), i))];
		int buddyFound = 0;

		struct list_head* counter;

		list_for_each(counter, &free_area[i]){
			if(buddy == list_entry(counter, page_t, list)){
				buddyFound = 1;
			}
		}

		if(buddyFound == 0){
			break;
		}

		list_del_init(&buddy->list);

		if(pageToFree->index > buddy->index){
			pageToFree = buddy;
		}
	}

	pageToFree->blockOrder = i;
	pageToFree->free = 1;
	list_add(&pageToFree->list, &free_area[i]);

}



/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
