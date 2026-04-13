// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "ShellDrag.h"

class DropSourceImpl : public IDropSource {
    ULONG refCount = 1;
public:
    HRESULT QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(IDropSource)) {
            *ppv = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG AddRef()  override { return ++refCount; }
    ULONG Release() override {
        ULONG rc = --refCount;
        if (rc == 0) delete this;
        return rc;
    }
    HRESULT QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed)             return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

// Creates a 70%-opacity top-down 32bpp DIB for use as a drag ghost image.
// The source must be a DIB section with 32bpp premultiplied pixels.
// The returned HBITMAP is owned by the caller (or transferred to IDragSourceHelper).
static HBITMAP CreateDragBitmap(HBITMAP hSrc, int w, int h) {
    if (!hSrc || w <= 0 || h <= 0) return nullptr;

    DIBSECTION ds = {};
    if (GetObject(hSrc, sizeof(ds), &ds) != sizeof(ds) ||
        !ds.dsBm.bmBits || ds.dsBmih.biBitCount != 32) return nullptr;

    int srcW    = ds.dsBm.bmWidth;
    int srcH    = std::abs(ds.dsBm.bmHeight);
    bool topDown = (ds.dsBmih.biHeight < 0);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;   // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pDst = nullptr;
    HBITMAP hDst = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &pDst, nullptr, 0);
    if (!hDst || !pDst) return nullptr;

    auto* src = static_cast<BYTE*>(ds.dsBm.bmBits);
    auto* dst = static_cast<BYTE*>(pDst);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = x * srcW / w;
            int sy = y * srcH / h;
            int srcRow = topDown ? sy : (srcH - 1 - sy);
            const BYTE* sp = src + (size_t)srcRow * ds.dsBm.bmWidthBytes + (size_t)sx * 4;
            BYTE* dp = dst + ((size_t)y * w + x) * 4;
            // 70% opacity: scale all channels (premultiplied, so RGB scales with alpha)
            dp[0] = (BYTE)((int)sp[0] * 7 / 10);
            dp[1] = (BYTE)((int)sp[1] * 7 / 10);
            dp[2] = (BYTE)((int)sp[2] * 7 / 10);
            dp[3] = (BYTE)((int)sp[3] * 7 / 10);
        }
    }
    return hDst;
}

void DoShellDrag(HWND hwndOwner, const std::wstring& filePath, HBITMAP hIcon, int iconSize) {
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(filePath.c_str());
    if (!pidl) return;

    PIDLIST_ABSOLUTE pidlParent = ILClone(pidl);
    ILRemoveLastID(pidlParent);

    IShellFolder* pDesktop = nullptr;
    SHGetDesktopFolder(&pDesktop);

    IShellFolder* pParent = nullptr;
    if (pidlParent && ILGetSize(pidlParent) > 2)
        pDesktop->BindToObject(pidlParent, nullptr, IID_PPV_ARGS(&pParent));

    IShellFolder* pFolder   = pParent ? pParent : pDesktop;
    LPCITEMIDLIST childPidl = ILFindLastID(pidl);

    IDataObject* pDataObj = nullptr;
    HRESULT hr = pFolder->GetUIObjectOf(hwndOwner, 1, &childPidl,
                                         IID_IDataObject, nullptr, (void**)&pDataObj);

    if (pParent)  pParent->Release();
    if (pDesktop) pDesktop->Release();
    ILFree(pidlParent);
    ILFree(pidl);

    if (FAILED(hr) || !pDataObj) return;

    // Attach a semi-transparent ghost drag image via IDragSourceHelper
    if (hIcon && iconSize > 0) {
        IDragSourceHelper* pHelper = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_IDragSourceHelper, (void**)&pHelper)) && pHelper) {
            HBITMAP hDragBmp = CreateDragBitmap(hIcon, iconSize, iconSize);
            if (hDragBmp) {
                SHDRAGIMAGE sdi        = {};
                sdi.sizeDragImage.cx   = iconSize;
                sdi.sizeDragImage.cy   = iconSize;
                sdi.ptOffset.x         = iconSize / 2;
                sdi.ptOffset.y         = iconSize / 2;
                sdi.hbmpDragImage      = hDragBmp;
                sdi.crColorKey         = CLR_NONE;  // use alpha channel
                // InitializeFromBitmap takes ownership of hDragBmp on success;
                // on failure we must free it ourselves.
                if (FAILED(pHelper->InitializeFromBitmap(&sdi, pDataObj)))
                    DeleteObject(hDragBmp);
            }
            pHelper->Release();
        }
    }

    DropSourceImpl* pDropSource = new DropSourceImpl();
    DWORD dwEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    DWORD dwEffectUsed = 0;
    // SHDoDragDrop provides full shell drag-image support on Windows Vista+
    SHDoDragDrop(hwndOwner, pDataObj, pDropSource, dwEffect, &dwEffectUsed);

    pDropSource->Release();
    pDataObj->Release();
}
