// LineDrawSever.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "framework.h"
#include "LineDrawSever.h"
#include <stdio.h>
#include <WinSock2.h>
#include "LogManager.h"
#include "SocketLog.h"
#include <iostream>
#include "RingBuffer.h"
#include <list>

#define MAX_LOADSTRING 100
#define SEVER_PORT 25000
#define WM_NETWORK WM_USER+1
#define MAX_USER 20
// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

//------------------------------------------------------------------------
// Juhup Space
//------------------------------------------------------------------------
struct Header
{
    unsigned short len;
};
struct DrawPacket
{
    int startX;
    int startY;
    int endX;
    int endY;
};
struct Session
{
    SOCKET socket;
    WCHAR ip[16];
    unsigned short port;
    RingBuffer recvRingBuffer;
    RingBuffer sendRingBuffer;
    bool bUsed;
};
SOCKET g_ListenSocket;

HWND g_hWnd;
Session g_SessionArray[MAX_USER] = { 0, };
int g_SessionCount=0;


void Network_Init(HWND hWnd);
void SelectProcess(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AcceptUser();
void ReadEvent(WPARAM wParam);
void WriteEvent(WPARAM wParam);
void BroadCastSend(char* packet, int len);
void Disconnect(Session* session);

void RecvPacket(Session* session);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 여기에 코드를 입력합니다.

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LINEDRAWSEVER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LINEDRAWSEVER));

    MSG msg;

    Network_Init(g_hWnd);
    // 기본 메시지 루프입니다:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LINEDRAWSEVER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LINEDRAWSEVER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    g_hWnd = hWnd;

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 메뉴 선택을 구문 분석합니다:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_NETWORK:
        SelectProcess(hWnd, message, wParam, lParam);
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
   
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void Network_Init(HWND hWnd)
{
    WSAData wsaData;
    SOCKADDR_IN serverAddr;

    if (0 != WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        ERROR_LOG(L"WSAStartup() Error", hWnd);
    }

    g_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (INVALID_SOCKET == g_ListenSocket)
    {
        ERROR_LOG(L"socket() Error", hWnd);
    }

    memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SEVER_PORT);
    serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    if (0 != bind(g_ListenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
    {
        ERROR_LOG(L"bind() Error", hWnd);
    }

    if (0 != listen(g_ListenSocket, SOMAXCONN))
    {
        ERROR_LOG(L"listen() Error", hWnd);
    }

    if (0 != WSAAsyncSelect(g_ListenSocket, hWnd, WM_NETWORK, FD_ACCEPT))
    {
        ERROR_LOG(L"WSAAsyncSelect error", hWnd);
    }

}

void SelectProcess(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (WSAGETSELECTERROR(lParam))
    {
        ERROR_LOG_SELECT(L"SelectProcess() error ", hWnd, WSAGETSELECTERROR(lParam));
    }
    switch (WSAGETSELECTEVENT(lParam))
    {
    case FD_ACCEPT:
        AcceptUser();
        break;
    case FD_READ:
        ReadEvent(wParam);
        break;
    case FD_WRITE:
        WriteEvent(wParam);
        break;
    case FD_CLOSE:
        break;
    }
}

void AcceptUser()
{
    SOCKADDR_IN clientAddr;
    int addrLen = sizeof(SOCKADDR_IN);
    SOCKET sessionSocket = accept(g_ListenSocket, (sockaddr*)&clientAddr, &addrLen);
    if (sessionSocket == INVALID_SOCKET)
    {
        ERROR_LOG(L"accept Error()", g_hWnd);
    }
    //--------------------------------------------------
    // 연결이 들어왔으면 세션을 만들고 WSAAsyncSelect로 이벤트를 등록해줘야함.
    //--------------------------------------------------

    InetNtop(AF_INET, &clientAddr.sin_addr.S_un.S_addr, g_SessionArray[g_SessionCount].ip, 16);
    g_SessionArray[g_SessionCount].port = ntohs(clientAddr.sin_port);
    g_SessionArray[g_SessionCount].socket = sessionSocket;
    g_SessionArray[g_SessionCount].bUsed = true;
    g_SessionCount++;

    if (0 != WSAAsyncSelect(sessionSocket, g_hWnd, WM_NETWORK, FD_READ | FD_WRITE | FD_CLOSE))
    {
        ERROR_LOG(L"WSAAsyncSelect Error", g_hWnd);
    }

    //--------------------------------------------------
    // 다른서버였다면 신규유저접속을 기존유저들에게 알리고, 기존유저의 상태를 신규유저에게줫을테지만...여긴그런거없다.
    //--------------------------------------------------
}

void ReadEvent(WPARAM wParam)
{
    bool bSearch = false;

    for(auto session : g_SessionArray)
    {
        if (session.socket == wParam)
        {
            RecvPacket(&session);
            bSearch = true;
            break;
        }
    }
    if (bSearch == false)
    {
        ERROR_LOG(L"Socket Search Fail (WARNNING!!", g_hWnd);
        int* p = 0;
        *p = 10;
    }
}

void WriteEvent(WPARAM wParam)
{
    bool bSearch = false;
    Session* searchedSession = nullptr;
    for (auto session : g_SessionArray)
    {
        if (session.socket == wParam)
        {
            searchedSession = &session;
            bSearch = true;
            break;
        }
    }
    if (bSearch == false)
    {
        ERROR_LOG(L"Socket Search Fail (WARNNING!!", g_hWnd);
        int* p = 0;
        *p = 10;
    }

    //-----------------------------------------------
    // 해당 송신버퍼에있는 데이터를 디큐해서 send한다
    //-----------------------------------------------
    char buffer[1000];

    while (searchedSession->sendRingBuffer.GetUsedSize() != 0)
    {
        int peekRtn = searchedSession->sendRingBuffer.Peek(buffer, searchedSession->sendRingBuffer.GetDirectDequeueSize());
        int sendRtn = send(searchedSession->socket, buffer, peekRtn, 0);
        if (sendRtn < 0)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                Disconnect(searchedSession);
                break;
            }
            ERROR_LOG(L"SendError", g_hWnd);
        }
        //------------------------------------
        // 만일 sendRtn이 PeekRtn보다 작을수있는데, 그럴땐 sendRtn만큼 디큐를 한다
        searchedSession->sendRingBuffer.MoveFront(sendRtn);
    }
}

void BroadCastSend(char* packet, int len)
{
    //------------------------------------
    // 바로보내는게아니고, 송신링버퍼에다 쓴다.
    //------------------------------------

    for (size_t i = 0; i < g_SessionCount; i++)
    {
        if (g_SessionArray[i].bUsed == true)
        {
            Header header;
            header.len = len;
            int enqRtn = g_SessionArray[i].sendRingBuffer.Enqueue((char*)&header, sizeof(Header));
            enqRtn = g_SessionArray[i].sendRingBuffer.Enqueue(packet, len);
            WriteEvent(g_SessionArray[i].socket);
        }
    }
    
}

void Disconnect(Session* session)
{
    if (session->bUsed == false)
    {
        return;
    }
    LINGER linger;
    linger.l_linger = 0;
    linger.l_onoff = 1;

    if (0 != setsockopt(session->socket, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(LINGER)))
    {
        ERROR_LOG(L"LInger() Error", g_hWnd);
    }

    closesocket(session->socket);
   
    ERROR_LOG(L"Client [%s][%d] closesocket", g_hWnd);

    session->bUsed = false;
    --g_SessionCount;
}

void RecvPacket(Session* session)
{
    int directEnqueueSize = session->recvRingBuffer.GetDirectEnqueueSize();

    int recvRtn = recv(session->socket, session->recvRingBuffer.GetRearBufferPtr(), directEnqueueSize, 0);

    if (recvRtn <= 0)
    {
        ERROR_LOG(L"Recv Error", g_hWnd);
        Disconnect(session);
        return;
    }
    session->recvRingBuffer.MoveRear(recvRtn);

    //-------------------------------------------
    // 패킷확인하면서 뽑기.
    //-------------------------------------------

    while(true)
    {
        if (session->recvRingBuffer.GetUsedSize() < sizeof(Header))
        {
            break;
        }

        Header header;

        int peekRtn = session->recvRingBuffer.Peek((char*)&header, sizeof(header));

        if (session->recvRingBuffer.GetUsedSize() < sizeof(Header)+ header.len)
        {
            break;
        }
        session->recvRingBuffer.MoveFront(sizeof(Header));

        DrawPacket drawPacket;

        int deqRtn = session->recvRingBuffer.Dequeue((char*)&drawPacket, sizeof(DrawPacket));

        BroadCastSend((char*)&drawPacket, sizeof(drawPacket));
    }


}
