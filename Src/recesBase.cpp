﻿//reces.cpp
//recesベース

//`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`
//              reces Ver.0.00r25 by x@rgs
//              under NYSL Version 0.9982
//
//`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`~^`

#include"Stdafx.h"
#include"reces.h"
#include"ArcTar32.h"
#include"Arc7-zip32.h"
#include"ArcUnrar32.h"
#include"ArcUnlha32.h"
#ifndef _WIN64
#include"ArcUniso32.h"
#include"ArcXacrett.h"
#endif
#include"Spi.h"
#include"Wcx.h"
#include"ParseOptions.h"


using namespace sslib;


//あかんやつ
std::vector<ArcDll*> RecesBase::m_arcdll_list;
ArcB2e* RecesBase::m_b2e_dll;
ArcDll* RecesBase::m_cal_dll;
std::vector<Spi*> RecesBase::m_spi_list;
std::vector<Wcx*> RecesBase::m_wcx_list;
tstring RecesBase::m_split_temp_dir;
tstring RecesBase::m_original_cur_dir;
RecesBase::CUR_FILE RecesBase::m_cur_file;
Archiver* RecesBase::m_arc_dll;
misc::thread::INFO RecesBase::m_progressbar_thread;


namespace{
	const UINT WM_HOOKDIALOG=::RegisterWindowMessage(_T("_WM_HOOKDIALOG_"));
	const UINT WM_CREATE_PROGRESSBAR=::RegisterWindowMessage(_T("WM_CREATE_PROGRESSBAR"));
	const UINT WM_UPDATE_PROGRESSBAR_MAIN=::RegisterWindowMessage(_T("WM_UPDATE_PROGRESSBAR_MAIN"));
	const UINT WM_DESTROY_PROGRESSBAR=::RegisterWindowMessage(_T("WM_DESTROY_PROGRESSBAR"));
}

tstring removeTailCharacter(const tstring& str,TCHAR c){
	if(str.empty())return str;

	tstring result(str);

	if(result.find_last_of(c)==result.length()-1){
		result=result.substr(0,result.length()-1);
	}
	return result;
}

//拡張子を取得(tar系考慮)、含まれなければ""を返す
tstring getExtensionEx(const tstring& file_path){
	if(file_path.empty())return file_path;

	tstring modified_path(path::removeExtension(file_path));

	if(modified_path==file_path)return _T("");

	//拡張子が'.tar.xz'等の場合、二度removeExtension()する
	if(path::getExtension(modified_path)==_T("tar")){
		modified_path=path::removeExtension(modified_path);
	}
	return file_path.substr(modified_path.length()+1);
}

//拡張子を削除(tar系考慮)
tstring removeExtensionEx(const tstring& file_path){
	if(file_path.empty())return file_path;

	tstring modified_path(path::removeExtension(file_path));

	//拡張子が'.tar.xz'等の場合、二度removeExtension()する
	if(path::getExtension(modified_path)==_T("tar")){
		return path::removeExtension(modified_path);
	}
	return modified_path;
}

bool info(const TCHAR* msg,...){
	if(CFG.no_display.no_information)return false;

	DWORD written_chars=0;

	va_list argp;
	va_start(argp,msg);

	STDOUT.write(msg,argp,&written_chars);

	va_end(argp);
	return true;
}

void errmsg(const TCHAR* msg,...){
	if(!strvalid(msg)||
	   CFG.no_display.no_errmsg)return;

	Console std_err_handle(STD_ERROR_HANDLE);
	DWORD written_chars=0;

	//文字色の変更
	std_err_handle.setFGColor(Console::HIGH_RED);

	va_list argp;
	va_start(argp,msg);

	std_err_handle.outputString(_T("error: "));
	std_err_handle.write(msg,argp,&written_chars);

	va_end(argp);

	//文字色を元に戻す
	std_err_handle.resetColors();
	return;
}

//'od'と'of'を反映した作成する書庫のパスを作成
RecesBase::ARC_RESULT RecesBase::arcFileName(CUR_FILE* new_cur_file,const tstring& arc_path,tstring& err_msg){
	//オプションの有無にかかわらず、出力先ディレクトリをoutput_dirに代入
	tstring output_dir(path::getParentDirectory(arc_path));
	new_cur_file->arc_path.assign(arc_path);

	if(!CFG.general.output_dir.empty()){
		//'od'
		if(path::isRelativePath(CFG.general.output_dir.c_str())||
		   !path::isAbsolutePath(CFG.compress.output_file.c_str())){
			//相対パスであれば絶対パスを作成
			std::vector<TCHAR> buffer(MAX_PATHW);

			path::getFullPath(&buffer[0],
							  buffer.size(),
							  CFG.general.output_dir.c_str(),
							  ((!CFG.general.default_base_dir)?
							   output_dir.c_str():
							   m_original_cur_dir.c_str()));
			output_dir.assign(&buffer[0]);
		}else{
			//絶対パス[\Dir含む]であればそのまま代入
			output_dir=CFG.general.output_dir;
		}

		if(!path::fileExists(output_dir.c_str())){
			if(!fileoperation::createDirectory(output_dir.c_str())){
				err_msg.assign(format(_T("ディレクトリ '%s' の作成に失敗しました。\n"),output_dir.c_str()));
				return ARC_CANNOT_CREATE_DIRECTORY;
			}
		}

		if(CFG.compress.output_file.empty()){
			//'of'が無効であればarc_pathを作成
			output_dir=path::addTailSlash(output_dir);
			new_cur_file->arc_path=output_dir+path::getFileName(arc_path);
		}
	}

	if(!CFG.compress.output_file.empty()){
		//'of'
		if(path::isRelativePath(CFG.compress.output_file.c_str())||
		   !path::isAbsolutePath(CFG.compress.output_file.c_str())){
			//相対パスであれば絶対パスを作成
			std::vector<TCHAR> buffer(MAX_PATHW);

			path::getFullPath(&buffer[0],
							  buffer.size(),
							  CFG.compress.output_file.c_str(),
							  ((!CFG.general.default_base_dir)?
							   output_dir.c_str():
							   m_original_cur_dir.c_str()));
			new_cur_file->arc_path.assign(&buffer[0]);
		}else{
			//絶対パス[\Dir含む]であればそのまま
			new_cur_file->arc_path=CFG.compress.output_file;
		}
		//修正したパスから親ディレクトリを取得
		output_dir=path::getParentDirectory(arc_path);

		if(!path::fileExists(output_dir.c_str())){
			if(!fileoperation::createDirectory(output_dir.c_str())){
				err_msg=format(_T("ディレクトリ '%s' の作成に失敗しました。\n"),output_dir.c_str());
				return ARC_CANNOT_CREATE_DIRECTORY;
			}
		}
	}

	bool need_ext=true;
	tstring ext;

	if(!CFG.compress.raw_file_name){
		//new_cur_file->arc_pathに圧縮形式の拡張子があれば追加しない
		if(m_arc_dll->isSupportedExtension(path::getExtension(new_cur_file->arc_path).c_str())){
			need_ext=false;
		}

		if(!CFG.compress.output_file.empty()){
			//'/of～'に圧縮形式の拡張子が含まれていれば追加しない
			if(m_arc_dll->isSupportedExtension((str::toLower(path::getExtension(CFG.compress.output_file))).c_str())){
				need_ext=false;
			}
		}
	}else{
		need_ext=false;
	}

	if(need_ext){
		if(m_arc_dll->type()==Archiver::CAL){
			ext=static_cast<ArcDll*>(m_arc_dll)->getMethod().ext;
		}
	}

	if(CFG.general.auto_rename){
		//既に存在する様であればリネーム(arc.zip->arc_1.zip)
		if(CFG.compress.raw_file_name){
			tstring e=getExtensionEx(CFG.compress.output_file);

			if(!e.empty()&&
			   new_cur_file->arc_path.rfind(str::toLower(e))!=tstring::npos){
				ext.assign(_T("."));
				ext+=getExtensionEx(CFG.compress.output_file);
				new_cur_file->arc_path=removeExtensionEx(new_cur_file->arc_path);
				need_ext=true;
			}
		}

		if(path::fileExists((new_cur_file->arc_path+((need_ext)?ext:_T(""))).c_str())){
			for(unsigned long long i=1;i<ULLONG_MAX;++i){
				tstring n=format(_T("_%I64u"),i);

				if(!path::fileExists((new_cur_file->arc_path+n.c_str()+((need_ext)?ext:_T(""))).c_str())){
					new_cur_file->arc_path+=n.c_str();
					new_cur_file->auto_renamed=true;
					break;
				}
			}
		}
	}

	if(need_ext&&
	   m_arc_dll->type()==Archiver::CAL){
		//拡張子を追加
		new_cur_file->arc_path+=static_cast<ArcDll*>(m_arc_dll)->getMethod().ext;
	}

	return ARC_SUCCESS;
}

//m_arcdll_list読み込み
void RecesBase::loadArcLib(){
	struct l{
		static bool loadUnBypass(ArcDll* list){
			if(!list->isLoaded()){
				return list->load(_T("UNBYPASS"),list->prefix().c_str());
			}
			return false;
		}
	};
	if(m_arcdll_list.size()!=0||
	   m_b2e_dll!=NULL)freeArcLib();

	m_arcdll_list.push_back(new ArcUnlha32());
#ifdef _WIN64
	l::loadUnBypass(m_arcdll_list.back());
#endif

	m_arcdll_list.push_back(new ArcUnrar32());
	m_arcdll_list.push_back(new ArcTar32());

#ifndef _WIN64
	m_arcdll_list.push_back(new ArcUniso32());
#endif

	m_arcdll_list.push_back(new Arc7zip32());

#ifndef _WIN64
	m_arcdll_list.push_back(new ArcXacrett());
#endif

#ifndef _WIN64
	//B2e.dllは特別扱い
	m_b2e_dll=new ArcB2e();
#endif
}

//m_arcdll_list解放
void RecesBase::freeArcLib(){
	for(size_t i=0,list_size=m_arcdll_list.size();i<list_size;i++){
		SAFE_DELETE(m_arcdll_list[i]);
	}

	std::vector<ArcDll*>().swap(m_arcdll_list);

#ifndef _WIN64
	if(m_b2e_dll!=NULL){
		SAFE_DELETE(m_b2e_dll);
	}
#endif
}

//読み込みと対応チェック
template<typename I>Archiver* RecesBase::loadAndCheck(I ite,I end,const TCHAR* arc_path,bool* loaded_library,const TCHAR* ext,const TCHAR* libname,const TCHAR* full_libname){
	for(;ite!=end;++ite){
		if(!(ext&&!(*ite)->isSupportedExtension(ext))&&
		   !(!ext&&libname!=NULL&&!str::isEqualStringIgnoreCase((*ite)->name(),libname))){
			if(!(!(*ite)->isLoaded()&&!(*ite)->load((full_libname!=NULL)?full_libname:libname,NULL))){
				if(loaded_library!=NULL)*loaded_library=true;
				if(!(arc_path!=NULL&&!(*ite)->isSupportedArchive(arc_path))){
					return *ite;
				}else{
					if(libname!=NULL)return NULL;
				}
			}
		}
	}
	return NULL;
}
template Archiver* RecesBase::loadAndCheck(std::vector<Archiver*>::iterator ite,std::vector<Archiver*>::iterator end,const TCHAR* arc_path,bool* loaded_library,const TCHAR* ext,const TCHAR* libname,const TCHAR* full_libname);
template Archiver* RecesBase::loadAndCheck(std::vector<ArcDll*>::iterator ite,std::vector<ArcDll*>::iterator end,const TCHAR* arc_path,bool* loaded_library,const TCHAR* ext,const TCHAR* libname,const TCHAR* full_libname);
template Archiver* RecesBase::loadAndCheck(std::vector<Spi*>::iterator ite,std::vector<Spi*>::iterator end,const TCHAR* arc_path,bool* loaded_library,const TCHAR* ext,const TCHAR* libname,const TCHAR* full_libname);
template Archiver* RecesBase::loadAndCheck(std::vector<Wcx*>::iterator ite,std::vector<Wcx*>::iterator end,const TCHAR* arc_path,bool* loaded_library,const TCHAR* ext,const TCHAR* libname,const TCHAR* full_libname);

//spiやwcxなどプラグインの読み込みと対応チェック
template<typename T>Archiver* RecesBase::loadAndCheckPlugin(std::vector<T*>* plugin_list,const TCHAR* arc_path,bool* loaded_library,const tstring& plugin_dir,const TCHAR* libname,Archiver::ARC_TYPE type){
	//フルパスを取得
	std::vector<TCHAR> full_path(MAX_PATH);

	if(path::getFullPath(&full_path[0],full_path.size(),libname)&&
	   path::fileExists(&full_path[0])){
		T* plugin=new T(&full_path[0]);

		if(plugin->type()==type){
			if(plugin_list!=NULL)plugin_list->insert(plugin_list->begin(),plugin);
		}else{
			SAFE_DELETE(plugin);
			return NULL;
		}
	}else{
		//プラグインディレクトリ以下にあると仮定
		if(!plugin_dir.empty()&&
			path::getFullPath(&full_path[0],full_path.size(),libname,plugin_dir.c_str())){
		}
	}
	return loadAndCheck(plugin_list->begin(),
						plugin_list->end(),
						arc_path,
						loaded_library,
						NULL,
						&full_path[0]);
}
template Archiver* RecesBase::loadAndCheckPlugin(std::vector<Spi*>* plugin_list,const TCHAR* arc_path,bool* loaded_library,const tstring& plugin_dir,const TCHAR* libname,Archiver::ARC_TYPE type);
template Archiver* RecesBase::loadAndCheckPlugin(std::vector<Wcx*>* plugin_list,const TCHAR* arc_path,bool* loaded_library,const tstring& plugin_dir,const TCHAR* libname,Archiver::ARC_TYPE type);

//ファイルのフルパスリストを作成
bool RecesBase::fullPathList(std::list<tstring>& list,std::vector<tstring>& filepaths,bool recursive){
	for(std::vector<tstring>::size_type i=0,size=filepaths.size();!isTerminated()&&i<size;++i){
		std::vector<TCHAR> buffer(MAX_PATHW);

		//フルパスを取得
		if(path::getFullPath(&buffer[0],buffer.size(),filepaths[i].c_str())&&
		   path::fileExists(&buffer[0])){
			tstring buffer_str(&buffer[0]);

			if(recursive){
				//ディレクトリなら再帰的検索してリストに追加
				if(path::isDirectory(&buffer[0])){
					path::recursiveSearch(&list,&buffer[0]);
					continue;
				}
			}

			if(find(list.begin(),list.end(),buffer_str)==list.end()){
				//リストに追加
				list.push_back(buffer_str);
			}
		}
	}
	return !list.empty();
}