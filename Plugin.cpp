/*
 * PROJECT:     PPEE (puppy) RSym plugin
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Parsing / pretty-printing the RSym symbols
 * COPYRIGHT:   Copyright 2020 Mark Jansen (mark.jansen@reactos.org)
 */

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <Windows.h>
#include <Unknwn.h>
#include <stdio.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
#include "rsym.h"
#pragma comment(lib, "Shlwapi.lib")


static BOOL g_bParsed = false;
static PBYTE g_RSymData = NULL;
static WCHAR g_ErrorText1[100];
static WCHAR g_ErrorText2[100];


// Helper function
void AddListItem(HWND hListView, LPCWSTR Member, LPCWSTR Value = NULL, LPCWSTR Comment = NULL)
{
    LVITEM item = { 0 };

    item.mask = 0;
    item.iItem = ListView_GetItemCount(hListView);

    if (ListView_InsertItem(hListView, &item) == -1)
        DebugBreak();

    ListView_SetItemText(hListView, item.iItem, 1, (WCHAR*)Member);
    if (Value && *Value)
        ListView_SetItemText(hListView, item.iItem, 2, (WCHAR*)Value);
    if (Comment && *Comment)
        ListView_SetItemText(hListView, item.iItem, 3, (WCHAR*)Comment);
}

static PIMAGE_SECTION_HEADER Find_RSym(const BYTE* MemBase)
{
    PIMAGE_DOS_HEADER DosHeader;
    DosHeader = (PIMAGE_DOS_HEADER)MemBase;
    if (DosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        wcscpy_s(g_ErrorText1, L"Invalid DOS signature");
        swprintf_s(g_ErrorText2, L"0x%04x", (ULONG)(USHORT)DosHeader->e_magic);
        return NULL;
    }

    PIMAGE_NT_HEADERS NtHeader;
    NtHeader = (PIMAGE_NT_HEADERS)(MemBase + DosHeader->e_lfanew);
    if (NtHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        wcscpy_s(g_ErrorText1, L"Invalid NT signature");
        swprintf_s(g_ErrorText2, L"0x%08x", NtHeader->Signature);
        return NULL;
    }

    PIMAGE_SECTION_HEADER SectionHeader;
    SectionHeader = IMAGE_FIRST_SECTION(NtHeader);
    for (ULONG n = 0; n < NtHeader->FileHeader.NumberOfSections; ++n)
    {
        if (!strncmp((const char*)SectionHeader[n].Name, ".rossym", IMAGE_SIZEOF_SHORT_NAME))
        {
            return SectionHeader + n;
        }
    }

    wcscpy_s(g_ErrorText1, L"No RSym data found");
    return NULL;
}

static void Parse_RSym(HWND hListView, const LPCWSTR FilterText)
{
    const ROSSYM_HEADER* RosSymHeader;
    const ROSSYM_ENTRY* First, *Last, *Entry;
    const CHAR* Strings;

    RosSymHeader = (const ROSSYM_HEADER*)g_RSymData;

    if (RosSymHeader->SymbolsOffset < sizeof(ROSSYM_HEADER)
        || RosSymHeader->StringsOffset < RosSymHeader->SymbolsOffset + RosSymHeader->SymbolsLength
        || 0 != (RosSymHeader->SymbolsLength % sizeof(ROSSYM_ENTRY)))
    {
        wcscpy_s(g_ErrorText1, L"Invalid ROSSYM_HEADER");
        return;
    }

    First = (const ROSSYM_ENTRY *)(g_RSymData + RosSymHeader->SymbolsOffset);
    Last = First + RosSymHeader->SymbolsLength / sizeof(ROSSYM_ENTRY);
    Strings = (const CHAR*)(g_RSymData + RosSymHeader->StringsOffset);

    WCHAR AddrBuffer[20];
    WCHAR SymbolBuffer[MAX_PATH * 2];
    WCHAR FileBuffer[MAX_PATH * 2];

    for (Entry = First; Entry != Last; Entry++)
    {
        const char* SymbolName = Strings + Entry->FunctionOffset;
        const char* File = Strings + Entry->FileOffset;

        swprintf_s(AddrBuffer, L"+ 0x%08x", Entry->Address);
        swprintf_s(SymbolBuffer, L"%S", SymbolName);

        if (Entry->SourceLine)
        {
            swprintf_s(FileBuffer, L"%S(%u)", File, Entry->SourceLine);
        }
        else
        {
            swprintf_s(FileBuffer, L"%S", File);
        }

        if (!FilterText || !FilterText[0] ||
            StrStrIW(SymbolBuffer, FilterText) ||
            StrStrIW(AddrBuffer, FilterText) ||
            StrStrIW(FileBuffer, FilterText))
        {
            AddListItem(hListView, SymbolBuffer, AddrBuffer, FileBuffer);
        }
    }
}

void Plugin_NodeActivated(HWND hListView, const BYTE* MemBase, const LPCWSTR FilterText)
{
    ListView_DeleteAllItems(hListView);

    if (!g_bParsed)
    {
        g_bParsed = true;

        PIMAGE_SECTION_HEADER SectionHeader;
        SectionHeader = Find_RSym(MemBase);
        if (SectionHeader)
        {
            ULONG_PTR RSymData = (ULONG_PTR)MemBase + SectionHeader->PointerToRawData;
            g_RSymData = (PBYTE)RSymData;
        }
    }

    if (g_RSymData)
    {
        // Lock window updates
        SendMessage(hListView, WM_SETREDRAW, FALSE, 0);

        // Parse the RSym data / add the items to the list view
        Parse_RSym(hListView, FilterText);

        // Unlock window updates
        SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
        // Refresh the window
        RedrawWindow(hListView, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    else
    {
        AddListItem(hListView, g_ErrorText1, g_ErrorText2);
    }
}

