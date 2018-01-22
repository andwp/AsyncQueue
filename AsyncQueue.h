/*
* 文件名： AsyncQueue.h
* 文件描述：异步队列管理
* 创建人：Pengrx 
* 创建日期：2016年12月13日
*/
#pragma once

#include <list>
#include <process.h>
using namespace std;
/*
* 队列通知接口
*/
template<class DataClass>
class IQueueNotify
{
public : 
	// 出对通知
	virtual void Dequeue(DataClass* pData) = 0 ;
};
/*
*  异步队列
*/
template<class DataClass, class InvokeClass>
class CAsyncQueue
{
// 定义类的方法
typedef void (InvokeClass::* CallbackFun)(DataClass *pDat) ; 
friend class CAsyncQueueTest;
private:
	InvokeClass* m_pInvokeClass;
	//线程句柄
	HANDLE m_hThreadHandle; 	
	//线程队列临界却
	CRITICAL_SECTION m_MessageDequeSection;  
	// 消息队列最大大小
	int m_iMaxQueueSize;
	// 队列通知
	IQueueNotify<DataClass>* m_pQueueCallback;
	// 队列缓存数据 
	list<DataClass*>  m_ListQueue;
	bool m_bStop; // 是否关闭
	CallbackFun m_pInvokeFun; // 外部调用方法地址 
	HANDLE m_hStartThdEvent; // 启动线程的event
	HANDLE m_hProcEvent; // 处理通知
public:
	// 最大队列个数
	CAsyncQueue(int iMaxQueueSize)
		:m_hThreadHandle(INVALID_HANDLE_VALUE)
		,m_iMaxQueueSize(iMaxQueueSize)
		,m_pQueueCallback(nullptr)
		,m_pInvokeFun(nullptr)
		,m_pInvokeClass(nullptr)
		,m_hStartThdEvent(INVALID_HANDLE_VALUE)
		,m_hProcEvent(INVALID_HANDLE_VALUE)
	{
		InitializeCriticalSection(&m_MessageDequeSection);
	};
private:
	// 队列锁
	void LockQueue()
	{
		EnterCriticalSection(&m_MessageDequeSection); 
	};
	// 解除队列锁
	void UnLockQueue()
	{
		LeaveCriticalSection(&m_MessageDequeSection);
	};
	// 启动处理线程
	bool StartProcThread()
	{
		if(INVALID_HANDLE_VALUE == m_hThreadHandle)
		{
			m_bStop  = false;
			union ThreadFnEnum
			{ 
				void (_cdecl *pCallBackFn)(LPVOID lpVoid);
				void (_cdecl CAsyncQueue::* pMemberFn)();
			}uThreadProc;

			uThreadProc.pMemberFn = &CAsyncQueue::ThreadProc; 
			m_hStartThdEvent = CreateEvent(NULL, false, false, NULL);
			m_hProcEvent= CreateEvent(NULL, true, false, NULL);
			m_hThreadHandle = (HANDLE)_beginthread(uThreadProc.pCallBackFn,0, this); 
			if(m_hThreadHandle != INVALID_HANDLE_VALUE)
			{    
				WaitForSingleObject(m_hStartThdEvent, INFINITE);  
			}
		}
		return (m_hThreadHandle != INVALID_HANDLE_VALUE); 
	};
	// 数据出队（非线程安全）
	DataClass* Dequeue()
	{
		DataClass *pDat = m_ListQueue.front();
		m_ListQueue.pop_front();  
		return pDat;
	};
	//************************************
	// 方法名称:    WaitForThreadExit 等待线程退出 ，单独调用会反生死锁
	// 全名:  CAsyncQueue<DataClass, InvokeClass>::WaitForThreadExit 
	// 返回类型:   void 
	//************************************
	void WaitForThreadExit()
	{ 
		if(INVALID_HANDLE_VALUE != m_hProcEvent)
		{
			SetEvent(m_hProcEvent); 
		}
		//等待线程退出 
		if(INVALID_HANDLE_VALUE != m_hStartThdEvent)
		{
			WaitForSingleObject(m_hStartThdEvent, INFINITE);
			CloseHandle(m_hStartThdEvent);
			m_hStartThdEvent = INVALID_HANDLE_VALUE;
		}
		if(INVALID_HANDLE_VALUE != m_hThreadHandle)
		{  
			m_hThreadHandle = INVALID_HANDLE_VALUE ;
		}
		if(INVALID_HANDLE_VALUE != m_hProcEvent)
		{ 
			CloseHandle(m_hProcEvent);
			m_hProcEvent = INVALID_HANDLE_VALUE;
		}
	};
public:
	// 启动队列 
	bool Start(CallbackFun pFun, InvokeClass* pInvokeClass)
	{
		m_pInvokeFun = pFun;
		m_pInvokeClass = pInvokeClass;
		bool bStartOk = StartProcThread();
		return bStartOk;
	};
	// 启动队列
	// 失败返回false
	bool Start(IQueueNotify<DataClass>* pCallback)
	{ 
		m_pQueueCallback  =  pCallback; 
		bool bStartOk = StartProcThread();
		return bStartOk;
	};
	 //   停止队列处理
	void Stop()
	{ 
		if(!m_bStop)
		{
			m_bStop = true; 
			ClearQueue();
			WaitForThreadExit();
			DeleteCriticalSection(&m_MessageDequeSection); 
		} 
		WaitForThreadExit();
	};
	// 数据入队(删除可在入队处理回调中执行，若入队失败，请立即删除！）
	bool Enqueue(DataClass* data)
	{
		bool bSuccess = false;
		LockQueue() ; 
		if( m_bStop == false && (m_pQueueCallback !=nullptr || m_pInvokeFun!=nullptr)) // 是否处理
		{ 
			if(m_ListQueue.size() <= m_iMaxQueueSize) // 队列是否达到门限
			{
				m_ListQueue.push_back(data);
				bSuccess  = true;
			}
		} 
		UnLockQueue();  
		SetEvent(m_hProcEvent); //ResumeThread(m_hThreadHandle); // 线程恢复
		return bSuccess ;
	};
	// 清空队列
	void ClearQueue()
	{  
		int iSize = m_ListQueue.size();
		while (iSize > 0)
		{
			LockQueue(); 
			iSize = m_ListQueue.size();
			if(iSize > 0)
			{
				DataClass *pDat = Dequeue();  
				if(pDat != nullptr)
				{
					delete pDat;
				}  
			} 
			UnLockQueue();  
			iSize = m_ListQueue.size();
		} 
	};
	virtual ~CAsyncQueue(void)
	{
		Stop();
	};
	
protected : 
	// 线程启动函数 
	void _cdecl ThreadProc()
	{ 
		SetEvent(m_hStartThdEvent);
		while(m_bStop == false)
		{
			DataClass* pData = nullptr;
			int iSize = m_ListQueue.size();
			if(iSize > 0)
			{ 
				if(m_bStop == true)
				{
					break;
				}
				LockQueue(); 
				iSize = m_ListQueue.size();
				if (iSize>0)
				{
					pData = Dequeue();
				}
				UnLockQueue();  
			} 
			if(!m_bStop && iSize > 0)
			{
				if(m_pQueueCallback !=nullptr)
				{
					m_pQueueCallback->Dequeue(pData);
				}
				if(m_pInvokeFun!=nullptr)
				{
					(m_pInvokeClass->*m_pInvokeFun)(pData);
				}
			}else
			{ 
				if(m_bStop && iSize >0 && pData!=nullptr)
				{
					delete pData;
					break;
				}
				ResetEvent(m_hProcEvent);
				if( m_ListQueue.size() <= 0 && !m_bStop )
				{
					WaitForSingleObject(m_hProcEvent, INFINITE);	//::SuspendThread(m_hThreadHandle);  // 线程挂起
				}
			}
		}  
		SetEvent(m_hStartThdEvent);
		_endthread();
	};
};

