//=====================================================================
//
// inetsim.c - network simulator for IP-layer
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "inetsim.h"


/*====================================================================*/
/* INTERFACE DEFINITION                                               */
/*====================================================================*/

//---------------------------------------------------------------------
// 单向链路：初始化
//---------------------------------------------------------------------
void isim_transfer_init(iSimTransfer *trans, long rtt, long lost, long amb, 
		long limit, int mode)
{
	assert(trans);
	trans->rtt = rtt;
	trans->lost = lost;
	trans->amb = amb;
	trans->limit = limit;
	trans->seed = 0;
	trans->size = 0;
	trans->current = 0;
	trans->cnt_send = 0;
	trans->cnt_drop = 0;
	trans->mode = mode;
	iqueue_init(&trans->head);
}

//---------------------------------------------------------------------
// 单向链路：销毁
//---------------------------------------------------------------------
void isim_transfer_destroy(iSimTransfer *trans)
{
	assert(trans);
	while (!iqueue_is_empty(&trans->head)) {
		struct IQUEUEHEAD *head = trans->head.next;
		iSimPacket *packet = iqueue_entry(head, iSimPacket, head);
		iqueue_del(head);
		free(packet);
	}
	trans->size = 0;
	trans->cnt_send = 0;
	trans->cnt_drop = 0;
	iqueue_init(&trans->head);
}

//---------------------------------------------------------------------
// 单向链路：设置时间
//---------------------------------------------------------------------
void isim_transfer_settime(iSimTransfer *trans, unsigned long time)
{
	assert(trans);
	trans->current = time;
}

//---------------------------------------------------------------------
// 单向链路：随机数
//---------------------------------------------------------------------
long isim_transfer_random(iSimTransfer *trans, long range)
{
	unsigned long seed, value;

	assert(trans);

	if (range <= 0) return 0;

	seed = trans->seed;
	value = (((seed = seed * 214013L + 2531011L) >> 16) & 0xffff);
	trans->seed = seed;

	return value % range;
}

//---------------------------------------------------------------------
// 单向链路：发送数据
//---------------------------------------------------------------------
long isim_transfer_send(iSimTransfer *trans, const void *data, long size)
{
	iSimPacket *packet;
	iqueue_head *p;
	long feature;
	long wave;

	trans->cnt_send++;

	// 判断是否超过限制
	if (trans->size >= trans->limit) {
		trans->cnt_drop++;
		return -1;
	}

	// 判断是否丢包
	if (trans->lost > 0) {
		if (isim_transfer_random(trans, 100) < trans->lost) {
			trans->cnt_drop++;
			return -2;
		}
	}

	// 分配新数据包
	packet = (iSimPacket*)malloc(sizeof(iSimPacket) + size);
	assert(packet);

	packet->data = ((unsigned char*)packet) + sizeof(iSimPacket);
	packet->size = size;

	memcpy(packet->data, data, size);

	// 计算到达时间
	wave = (trans->rtt * trans->amb) / 100;
	wave = (wave * (isim_transfer_random(trans, 200) - 100)) / 100;
	wave = wave + trans->rtt;

	if (wave < 0) feature = trans->current;
	else feature = trans->current + wave;

	packet->timestamp = feature;

	// 按到达时间先后插入时间链表
	for (p = trans->head.prev; p != &trans->head; p = p->prev) {
		iSimPacket *node = iqueue_entry(p, iSimPacket, head);
		if (node->timestamp < packet->timestamp) break;
	}
	
	// 如果是顺序模式
	if (trans->mode != 0) p = trans->head.prev;

	iqueue_add(&packet->head, p);
	trans->size++;

	return 0;
}

//---------------------------------------------------------------------
// 单向链路：接收数据
//---------------------------------------------------------------------
long isim_transfer_recv(iSimTransfer *trans, void *data, long maxsize)
{
	iSimPacket *packet;
	iqueue_head *p;
	long size = 0;

	assert(trans);

	// 没有数据包
	if (iqueue_is_empty(&trans->head)) {
		return -1;
	}

	p = trans->head.next;
	packet = iqueue_entry(p, iSimPacket, head);

	// 还为到达接收时间
	if (trans->current < packet->timestamp) {
		return -2;
	}

	// 移除队列
	iqueue_del(p);
	trans->size--;

	// 数据拷贝
	if (data) {
		size = packet->size;
		if (size > maxsize) size = maxsize;
		memcpy(data, packet->data, size);
	}

	// 释放内存
	free(packet);

	return size;
}


//---------------------------------------------------------------------
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
//---------------------------------------------------------------------
void isim_init(iSimNet *simnet, long rtt, long lost, long amb, long limit, int mode)
{
	assert(simnet);
	isim_transfer_init(&simnet->t1, rtt >> 1, lost, amb, limit, mode);
	isim_transfer_init(&simnet->t2, rtt >> 1, lost, amb, limit, mode);
	simnet->p1.t1 = &simnet->t1;
	simnet->p1.t2 = &simnet->t2;
	simnet->p2.t1 = &simnet->t2;
	simnet->p2.t2 = &simnet->t1;
}

//---------------------------------------------------------------------
// 删除网络模拟器
//---------------------------------------------------------------------
void isim_destroy(iSimNet *simnet)
{
	assert(simnet);
	assert(simnet->p1.t1 && simnet->p1.t2);
	assert(simnet->p2.t1 && simnet->p2.t2);

	isim_transfer_destroy(&simnet->t1);
	isim_transfer_destroy(&simnet->t2);

	simnet->p1.t1 = NULL;
	simnet->p1.t2 = NULL;
	simnet->p2.t1 = NULL;
	simnet->p2.t2 = NULL;
}

//---------------------------------------------------------------------
// 设置时间
//---------------------------------------------------------------------
void isim_settime(iSimNet *simnet, unsigned long current)
{
	assert(simnet);
	isim_transfer_settime(&simnet->t1, current);
	isim_transfer_settime(&simnet->t2, current);
}

//---------------------------------------------------------------------
// 发送数据
//---------------------------------------------------------------------
long isim_send(iSimPeer *peer, const void *data, long size)
{
	return isim_transfer_send(peer->t1, data, size);
}

//---------------------------------------------------------------------
// 接收数据
//---------------------------------------------------------------------
long isim_recv(iSimPeer *peer, void *data, long maxsize)
{
	return isim_transfer_recv(peer->t2, data, maxsize);
}

//---------------------------------------------------------------------
// 取得端点：peerno = 0(端点1), 1(端点2)
//---------------------------------------------------------------------
iSimPeer *isim_peer(iSimNet *simnet, int peerno)
{
	assert(simnet);
	assert(peerno == 0 || peerno == 1);
	if (peerno == 0) return &simnet->p1;
	return &simnet->p2;
}

//---------------------------------------------------------------------
// 设置随机数种子
//---------------------------------------------------------------------
void isim_seed(iSimNet *simnet, unsigned long seed1, unsigned long seed2)
{
	assert(simnet);
	simnet->t1.seed = seed1;
	simnet->t2.seed = seed2;
}

