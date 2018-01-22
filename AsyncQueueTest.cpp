#include "assert.h"
#include "AsyncQueueTest.h"

CAsyncQueueTest::CAsyncQueueTest(void)
{
	m_pQueue = new CAsyncQueue<int,CAsyncQueueTest>(nMaxQueueCnt);
	bool bStarted = m_pQueue->Start(&CAsyncQueueTest::ProcData, this);
	assert(true == bStarted);

}

CAsyncQueueTest::~CAsyncQueueTest(void)
{
  delete m_pQueue;
}


void CAsyncQueueTest::ProcData(int* pDat)
{ 
	char strTmpOut[100]; 
	sprintf(strTmpOut, "正在处理第%d个数据\r\n\0", *pDat);
	OutputDebugStringA(strTmpOut);
	Sleep(1);
	delete pDat;
}

void CAsyncQueueTest::DoTest()
{
	// test Enqueue
	int nMax = nMaxQueueCnt + 1;
	for (int i =1 ; i <=nMax ; i++ )
	{
		int *pData = new int(i);
		bool bOk = m_pQueue->Enqueue(pData);
		assert(bOk); 
	}
}
