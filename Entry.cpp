/*
 * PROJECT:     PPEE (puppy) RSym plugin
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Entrypoint / hooking into the PPEE treeview
 * COPYRIGHT:   Copyright 2020 Mark Jansen (mark.jansen@reactos.org)
 */

#include <windows.h>
#include <CommCtrl.h>
#include <cassert>
#pragma comment(lib, "Comctl32.lib")

extern "C"
IMAGE_DOS_HEADER __ImageBase;

static HTREEITEM g_PluginNode;
static const WCHAR* g_FileName;
static const BYTE* g_MemBase;

// The actual plugin :)
void Plugin_NodeActivated(HWND hListView, const WCHAR* FileName, const BYTE* g_MemBase, const LPCWSTR FilterText);


static
DWORD WINAPI ExitProc(LPVOID)
{
    // Make sure our SubclassProc is no longer executing
    Sleep(1000);

    // Cleanup
    FreeLibraryAndExitThread((HMODULE)&__ImageBase, 0);
}

static void CallPlugin(HWND hWnd)
{
    WCHAR FilterText[1024] = { 0 };

    // Get the info we need to call the plugin
    HWND hListView = FindWindowExW(hWnd, NULL, WC_LISTVIEWW, NULL);
    HWND hFilterEdit = FindWindowExW(hWnd, NULL, WC_EDIT, NULL);

    // Trivia: PPEE has a buffer overflow on this, it uses sizeof instead of countof :)
    GetWindowTextW(hFilterEdit, FilterText, _countof(FilterText));
    Plugin_NodeActivated(hListView, g_FileName, g_MemBase, FilterText);
}

static
LRESULT WINAPI SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (uMsg == WM_NOTIFY)
    {
        LPNMHDR header = (LPNMHDR)lParam;

        switch (header->code)
        {
        case TVN_SELCHANGED:
            if (((LPNMTREEVIEW)lParam)->itemNew.hItem == g_PluginNode ||
                header->idFrom == 112)   // Filter box change
            {
                // Give PPEE a chance to clear the ListView, add default headers etc.
                LRESULT lResult = DefSubclassProc(hWnd, uMsg, wParam, lParam);

                CallPlugin(hWnd);

                // Return whatever PPEE wanted to return
                return lResult;
            }
            break;
        case TVN_DELETEITEM:
            if (((LPNMTREEVIEW)lParam)->itemOld.hItem == g_PluginNode)
            {
                // Call the default handler
                LRESULT lResult = DefSubclassProc(hWnd, uMsg, wParam, lParam);

                // Remove our subclass
                RemoveWindowSubclass(hWnd, SubclassProc, 0);

                // If we unload ourself while in this callback we might try to execute to code that is unloaded,
                // so ask windows to do it for us from a new thread
                HANDLE hThread = CreateThread(NULL, NULL, ExitProc, NULL, 0, NULL);
                CloseHandle(hThread);

                // Bail out as fast as possible
                return lResult;
            }
            break;
        }
    }

    // Call the default handler
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// From 'Wolf' we can safely decrease the reference count
void Unload()
{
    FreeLibrary((HMODULE)&__ImageBase);
}


void AddTreeviewNode(HWND hMainWindow)
{
    HWND hTreeview = FindWindowExW(hMainWindow, NULL, WC_TREEVIEWW, NULL);

    if (!hTreeview)
    {
        OutputDebugStringA("Unable to find TreeView\n");
        Unload();
        return;

    }
    if (!SetWindowSubclass(hMainWindow, SubclassProc, 0, 0))
    {
        OutputDebugStringA("Unable to subclass window\n");
        Unload();
        return;
    }

    TVINSERTSTRUCT item = { 0 };

    item.hInsertAfter = TVI_LAST;
    item.item.mask = TVIF_TEXT;
    item.item.pszText = (WCHAR*)L"RSym";

    g_PluginNode = TreeView_InsertItem(hTreeview, &item);

    // Activate our node
    TreeView_SelectItem(hTreeview, g_PluginNode);
}

// Plugin entrypoint
void Wolf(HWND* hWndMainWindow, WCHAR* FileName, BYTE* memBase)
{
    // We are already loaded / hooked up, someone clicked on the plugin item...
    if (g_PluginNode)
    {
        assert(FileName == g_FileName);
        assert(memBase == g_MemBase);

        HWND hTreeview = FindWindowExW(*hWndMainWindow, NULL, WC_TREEVIEWW, NULL);
        // Is our node selected?
        if (TreeView_GetSelection(hTreeview) == g_PluginNode)
        {
            CallPlugin(*hWndMainWindow);
        }
        else
        {
            // Select it
            TreeView_SelectItem(hTreeview, g_PluginNode);
        }
        return;
    }


    WCHAR Buffer[MAX_PATH];

    // Increase reference count
    GetModuleFileNameW((HMODULE)&__ImageBase, Buffer, _countof(Buffer));
    LoadLibraryW(Buffer);

    // Store this for callbacks
    g_FileName = FileName;
    g_MemBase = memBase;

    // Add the actual plugin stuff
    AddTreeviewNode(*hWndMainWindow);
}

BOOL
WINAPI
DllMain(HMODULE hDll, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hDll);
    }
    return TRUE;
}


