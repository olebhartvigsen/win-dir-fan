#include "pch.h"
#include "ShellDrag.h"

// CLSID_DragDropHelper
static const GUID CLSID_DragDropHelper_ = {
    0x4657278A, 0x411B, 0x11D2,
    {0x83, 0x9A, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0}
};

// IID_IDragSourceHelper
static const GUID IID_IDragSourceHelper_ = {
    0xDE5BF786, 0x477A, 0x11D2,
    {0x83, 0x9D, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0}
};

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

    ULONG AddRef() override { return ++refCount; }

    ULONG Release() override {
        ULONG rc = --refCount;
        if (rc == 0) delete this;
        return rc;
    }

    HRESULT QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }

    HRESULT GiveFeedback(DWORD) override {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }
};

void DoShellDrag(HWND hwndOwner, const std::wstring& filePath) {
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(filePath.c_str());
    if (!pidl) return;

    // Build array of PIDLs
    LPCITEMIDLIST pidlArray[1] = { pidl };

    IDataObject* pDataObj = nullptr;
    // Get the parent folder and child PIDL
    PIDLIST_ABSOLUTE pidlParent = ILClone(pidl);
    ILRemoveLastID(pidlParent);

    IShellFolder* pDesktop = nullptr;
    SHGetDesktopFolder(&pDesktop);

    IShellFolder* pParent = nullptr;
    if (pidlParent && ILGetSize(pidlParent) > 2) {
        pDesktop->BindToObject(pidlParent, nullptr, IID_PPV_ARGS(&pParent));
    }

    IShellFolder* pFolder = pParent ? pParent : pDesktop;
    LPCITEMIDLIST childPidl = ILFindLastID(pidl);

    HRESULT hr = pFolder->GetUIObjectOf(hwndOwner, 1, &childPidl, IID_IDataObject, nullptr, (void**)&pDataObj);

    if (pParent) pParent->Release();
    if (pDesktop) pDesktop->Release();
    ILFree(pidlParent);
    ILFree(pidl);

    if (FAILED(hr) || !pDataObj) return;

    DropSourceImpl* pDropSource = new DropSourceImpl();
    DWORD dwEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    DWORD dwEffectUsed = 0;
    DoDragDrop(pDataObj, pDropSource, dwEffect, &dwEffectUsed);

    pDropSource->Release();
    pDataObj->Release();
}
