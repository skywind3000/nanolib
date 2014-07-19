//=====================================================================
//
// inetsim.h - network simulator for IP-layer
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#ifndef __INETSIM_H__
#define __INETSIM_H__


/*====================================================================*/
/* QUEUE DEFINITION                                                   */
/*====================================================================*/
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;


/*--------------------------------------------------------------------*/
/* queue init                                                         */
/*--------------------------------------------------------------------*/
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )

#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)


/*--------------------------------------------------------------------*/
/* queue operation                                                    */
/*--------------------------------------------------------------------*/
#define IQUEUE_ADD(node, head) ( \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) ( \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0);

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)


#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif

#endif



/*====================================================================*/
/* GLOBAL DEFINITION                                                  */
/*====================================================================*/

// 模拟数据包
struct ISIMPACKET
{
	struct IQUEUEHEAD head;			// 链表节点：按照时间排序
	unsigned long timestamp;		// 时间戳：到达的时间
	unsigned long size;				// 大小
	unsigned char *data;			// 数据指针
};

typedef struct ISIMPACKET iSimPacket;

// 单向链路
struct ISIMTRANSFER
{
	struct IQUEUEHEAD head;			// 时间排序链表
	unsigned long current;			// 当前时间
	unsigned long seed;				// 随机种子
	long size;						// 包个数
	long limit;						// 最大包数
	long rtt;						// 平均往返时间(120, 200, ..)
	long lost;						// 丢包率百分比(0-100)
	long amb;						// 延迟振幅百分比(0-100)
	int mode;						// 模式0(会前后到达)1(顺序到达)
	long cnt_send;					// 发送了多少个包
	long cnt_drop;					// 丢失了多少个包
};

typedef struct ISIMTRANSFER iSimTransfer;

// 网络端点
struct ISIMPEER
{
	iSimTransfer *t1;				// 发送链路
	iSimTransfer *t2;				// 接收链路
};

typedef struct ISIMPEER iSimPeer;

// 网络模拟器
struct ISIMNET
{
	iSimTransfer t1;				// 链路1
	iSimTransfer t2;				// 链路2
	iSimPeer p1;					// 端点1 (t1, t2)
	iSimPeer p2;					// 端点2 (t2, t1)
};

typedef struct ISIMNET iSimNet;


#ifdef __cplusplus
extern "C" {
#endif

/*====================================================================*/
/* INTERFACE DEFINITION                                               */
/*====================================================================*/

// 单向链路：初始化
void isim_transfer_init(iSimTransfer *trans, long rtt, long lost, long amb, 
		long limit, int mode);

// 单向链路：销毁
void isim_transfer_destroy(iSimTransfer *trans);

// 单向链路：设置时间
void isim_transfer_settime(iSimTransfer *trans, unsigned long time);

// 单向链路：随机数
long isim_transfer_random(iSimTransfer *trans, long range);

// 单向链路：发送数据
long isim_transfer_send(iSimTransfer *trans, const void *data, long size);

// 单向链路：接收数据
long isim_transfer_recv(iSimTransfer *trans, void *data, long maxsize);



// isim_init:
// 初始化网络模拟器
// rtt   - 往返时间平均数
// lost  - 丢包率百分比 (0 - 100)
// amb   - 时间振幅百分比 (0 - 100)
// limit - 最多包缓存数量
// mode  - 0(后发包会先到) 1(后发包必然后到达)
// 到达时间  = 当前时间 + rtt * 0.5 + rtt * (amb * 0.01) * random(-0.5, 0.5)
// 公网极速  = rtt( 60), lost( 5), amb(30), limit(1000)
// 公网快速  = rtt(120), lost(10), amb(40), limit(1000)
// 公网普通  = rtt(200), lost(10), amb(50), limit(1000)
// 公网慢速  = rtt(800), lost(20), amb(60), limit(1000)
void isim_init(iSimNet *simnet, long rtt, long lost, long amb, long limit, int mode);

// 删除网络模拟器
void isim_destroy(iSimNet *simnet);

// 设置时间
void isim_settime(iSimNet *simnet, unsigned long current);


// 发送数据
long isim_send(iSimPeer *peer, const void *data, long size);

// 接收数据
long isim_recv(iSimPeer *peer, void *data, long maxsize);

// 取得端点：peerno = 0(端点1), 1(端点2)
iSimPeer *isim_peer(iSimNet *simnet, int peerno);

// 设置随机数种子
void isim_seed(iSimNet *simnet, unsigned long seed1, unsigned long seed2);



#ifdef __cplusplus
}
#endif

#endif



