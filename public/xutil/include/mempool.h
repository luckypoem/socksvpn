#ifndef MEMPOOL_H_
#define MEMPOOL_H_

#include "list.h"
#include "commtype.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	struct list_head node;
	unsigned char data[0]; /*后面为key + obj*/
}MEM_NODE_T;


typedef struct
{
	/*节点个数*/
	unsigned int m_node_cnt;
	/*节点大小*/
	unsigned int m_node_size;
	/*初始化函数*/
	void (*node_init_func)(char*);

	/*内存池*/
	char *m_baseaddr;

	/*可用节点池堆栈*/
	MEM_NODE_T **m_stack;
	/*节点池堆栈当前位置*/
	unsigned int m_stack_idx;
}MEM_POOL_T;


DLL_API int mpool_init(MEM_POOL_T *pool,
		unsigned int obj_max_cnt,
		unsigned int node_size);

DLL_API void mpool_destroy(MEM_POOL_T *pool);
DLL_API void mpool_reset(MEM_POOL_T *pool);

DLL_API MEM_NODE_T* mpool_new_obj(MEM_POOL_T *pool);
DLL_API void mpool_free_obj(MEM_POOL_T *pool, MEM_NODE_T *node);

DLL_API void* mpool_malloc(MEM_POOL_T *pool);
DLL_API void mpool_free(MEM_POOL_T *pool, void *ptr);

DLL_API unsigned int mpoll_get_used_cnt(MEM_POOL_T *pool);


#ifdef __cplusplus
}
#endif

#endif /* MEMPOOL_H_ */
