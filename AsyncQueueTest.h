#pragma once
#include "AsyncQueue.h"
class CAsyncQueueTest
{
public:
	CAsyncQueueTest(void);
	~CAsyncQueueTest(void);
	void DoTest();
private: 
	void ProcData(int* pDat);
private:
	CAsyncQueue<int,CAsyncQueueTest>* m_pQueue;
};
