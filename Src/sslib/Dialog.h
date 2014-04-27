﻿//Dialog.h

#ifndef _DIALOG_H_3004F2FC_6FBB_4301_A1E3_B1B220211BFB
#define _DIALOG_H_3004F2FC_6FBB_4301_A1E3_B1B220211BFB

namespace sslib{
class Dialog{
public:
	Dialog(UINT resource_id,HINSTANCE instance=::GetModuleHandle(NULL)):
		m_resource_id(resource_id),
		m_instance(instance),
		m_dlg_handle(NULL),
		m_dlg_type(MODAL),
		m_exit_code(0){}
	virtual ~Dialog(){}

public:
	enum DLG_TYPE{MODAL,MODELESS};

private:
	const UINT m_resource_id;
	const HINSTANCE m_instance;
	HWND m_dlg_handle;
	DLG_TYPE m_dlg_type;
	UINT m_exit_code;
	void* m_param;

protected:
	inline void setHandle(HWND dlg_handle){m_dlg_handle=dlg_handle;}
	inline void setDlgType(DLG_TYPE dlg_type){m_dlg_type=dlg_type;}
	inline void setExitCode(UINT exit_code){m_exit_code=exit_code;}

	//メッセージハンドラ
	virtual bool onInitDialog(WPARAM wparam,LPARAM lparam);
	virtual bool onDestroy();
	virtual bool onOk();
	virtual bool onCancel();
	virtual bool onCommand(WPARAM wparam,LPARAM lparam);
	virtual bool onSysCommand(WPARAM wparam,LPARAM lparam);
	virtual bool onNotify(WPARAM wparam,LPARAM lparam);
	virtual bool onPaint();
	virtual bool onSize(WPARAM wparam,LPARAM lparam);
	virtual bool onMove(WPARAM wparam,LPARAM lparam);
	virtual bool onCtlColorStatic(WPARAM wparam,LPARAM lparam);
	virtual bool onDropFiles(HDROP drop_handle);
	virtual bool onMessage(UINT message,WPARAM wparam,LPARAM lparam);

public:
	//モーダルダイアログを表示
	int doModal(HWND parent_handle=NULL);
	//モードレスダイアログを表示
	HWND doModeless(HWND parent_handle=NULL,int cmd_show=SW_SHOW);
	//ダイアログを表示
	bool showDialog(int cmd_show=SW_SHOW);
	//ダイアログを閉じる
	bool endDialog(UINT exit_code=0);
	//最前面に表示
	bool topMost(bool top_most=true)const{
		return ::SetWindowPos(handle(),(top_most)?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE)!=0;
	}

	inline HWND getDlgItem(int id)const{return ::GetDlgItem(handle(),id);}

	LRESULT sendMessage(UINT message,WPARAM wparam=0,LPARAM lparam=0);
	LRESULT sendItemMessage(int item,UINT message,WPARAM wparam=0,LPARAM lparam=0);
	bool postMessage(UINT message,WPARAM wparam=0,LPARAM lparam=0);

	virtual INT_PTR handleMessage(HWND dlg_handle,UINT message,WPARAM wparam,LPARAM lparam);

	inline void setParam(void* param){m_param=param;}

	inline LRESULT setIcon(WORD id){
		return sendMessage(WM_SETICON,
						   ICON_BIG,
						   (LPARAM)::LoadIcon(inst(),MAKEINTRESOURCE(id)));
	}

	inline HWND handle()const{return m_dlg_handle;}
	inline HINSTANCE inst()const{return m_instance;}
	inline UINT id()const{return m_resource_id;}
	inline DLG_TYPE type()const{return m_dlg_type;}
	inline UINT exit_code()const{return m_exit_code;}
	inline void* param()const{return m_param;}
};

//namespace sslib
}

#endif //_DIALOG_H_3004F2FC_6FBB_4301_A1E3_B1B220211BFB
