/*
 * ftool-gui - 文件日期修改工具 (GUI版本 v7)
 * C + Win32 API
 * - 文件名直接转日期（选文件→点按钮→自动填入，无弹窗）
 * - 分别勾选创建/修改/访问时间（默认仅修改时间）
 * - 右键菜单删除单个文件
 * - 禁止窗口最大化，避免布局异常
 *
 * 编译: gcc -o ftool-gui.exe ftool-gui.c -static -O2 -s -municode -mwindows -lcomctl32 -lcomdlg32
 */

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>

/* ===== 资源 ID ===== */
#define ID_LISTVIEW      1001
#define ID_DATEPICKER    1002
#define ID_TIMEEDIT      1003
#define ID_BTN_ADD       1004
#define ID_BTN_CLEAR     1005
#define ID_BTN_SETNOW    1006
#define ID_BTN_SETSPEC   1007
#define ID_BTN_MINUS1D   1008
#define ID_BTN_MINUS1H   1009
#define ID_BTN_PLUS1H    1010
#define ID_BTN_PLUS1D    1011
#define ID_BTN_CUSTOM    1012
#define ID_CUSTOM_EDIT   1013
#define ID_STATUSBAR     1014
#define ID_BTN_SELALL    1015
#define ID_CHK_CREATE    1016
#define ID_CHK_MODIFY    1017
#define ID_CHK_ACCESS    1018
#define ID_BTN_FILEDATE  1019
#define ID_BTN_ADDDIR   1020
#define ID_BTN_RANDTIME 1021
#define ID_DATEPICKER2   1022
#define ID_CHK_RECURSIVE 1023
#define ID_C_DATEPICKER  1030
#define ID_C_TIMEEDIT    1031
#define ID_M_DATEPICKER  1032
#define ID_M_TIMEEDIT    1033
#define ID_A_DATEPICKER  1034
#define ID_A_TIMEEDIT    1035
#define ID_FILE_SELALL  3010
#define ID_FILE_INVERT  3011
#define IDI_APP_ICON    2000
#define ID_MENU_REMOVE   4000
#define ID_ED_CDATE     5001
#define ID_ED_CDATE_BTN 5002
#define ID_ED_MDATE     5003
#define ID_ED_MDATE_BTN 5004
#define ID_ED_ADATE     5005
#define ID_ED_ADATE_BTN 5006
#define ID_ED_CCHK      5007
#define ID_ED_MCHK      5008
#define ID_ED_ACHK      5009
#define ID_ED_OK        5010
#define ID_ED_CANCEL    5011
#define ID_ED_CTIME     5012
#define ID_ED_MTIME     5013
#define ID_ED_ATIME     5014
#define ID_ED_SYNC      5015
#define ID_ED_BOX       5099
#define ID_CTX_LOADTO  5100
#define ID_BTN_SYNCMAIN 1024  /* 右键：读取选定文件属性到配置区 */

#define MAX_FILES        4096
#define MASK_CREATE      1
#define MASK_MODIFY      2
#define MASK_ACCESS      4

typedef struct { WCHAR path[MAX_PATH]; FILETIME ftCreate; FILETIME ftModify; FILETIME ftAccess; } FileEntry;

static FileEntry  g_files[MAX_FILES];
static int        g_fileCount = 0;
static HWND       g_hListView, g_hDatePicker, g_hTimeEdit, g_hRandDatePicker;
static HWND       g_hCDatePicker, g_hMDatePicker, g_hADatePicker;
static HWND       g_hCTimeEdit, g_hMTimeEdit, g_hATimeEdit;
static HWND       g_hCustomEdit, g_hStatusBar, g_hMainWnd;
static HWND       g_hChkCreate, g_hChkModify, g_hChkAccess, g_hChkRecursive;
static HINSTANCE  g_hInst;
static HFONT      g_hFont;
static HBRUSH     g_hBgBrush;

/* ===== 文件时间 ===== */
static BOOL FileGetTimes(LPCWSTR path, FILETIME *pC, FILETIME *pM, FILETIME *pA) {
    HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(h==INVALID_HANDLE_VALUE)return FALSE; BOOL ok=GetFileTime(h,pC,pA,pM);CloseHandle(h);return ok;
}
static BOOL FileSetTimes(LPCWSTR path, const FILETIME *pC, const FILETIME *pM, const FILETIME *pA, int mask) {
    FILETIME oC,oM,oA; if(!FileGetTimes(path,&oC,&oM,&oA))return FALSE;
    FILETIME uC=(mask&1)?*pC:oC, uM=(mask&2)?*pM:oM, uA=(mask&4)?*pA:oA;
    HANDLE h=CreateFileW(path,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(h==INVALID_HANDLE_VALUE)return FALSE; BOOL ok=SetFileTime(h,&uC,&uA,&uM);CloseHandle(h);return ok;
}
static void FtToStr(FILETIME *ft, WCHAR *buf, int sz) { FILETIME lf;SYSTEMTIME st;FileTimeToLocalFileTime(ft,&lf);FileTimeToSystemTime(&lf,&st); _snwprintf(buf,sz,L"%04d-%02d-%02d %02d:%02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond); }
static LONGLONG FtTo100ns(const FILETIME *ft) { return ((LONGLONG)ft->dwHighDateTime<<32)|ft->dwLowDateTime; }
static void Ns100ToFt(LONGLONG v,FILETIME *ft) { ft->dwLowDateTime=(DWORD)(v&0xFFFFFFFF);ft->dwHighDateTime=(DWORD)(v>>32); }
static LONGLONG StToNs(SYSTEMTIME *st) { FILETIME lf,ft;SystemTimeToFileTime(st,&lf);LocalFileTimeToFileTime(&lf,&ft);return FtTo100ns(&ft); }

static void SetStatus(LPCWSTR msg) { SendMessageW(g_hStatusBar,SB_SETTEXTW,0,(LPARAM)msg); }
static int GetTimeMask(void) {
    int m=0; if(SendMessageW(g_hChkCreate,BM_GETCHECK,0,0)==BST_CHECKED)m|=MASK_CREATE;
    if(SendMessageW(g_hChkModify,BM_GETCHECK,0,0)==BST_CHECKED)m|=MASK_MODIFY;
    if(SendMessageW(g_hChkAccess,BM_GETCHECK,0,0)==BST_CHECKED)m|=MASK_ACCESS; return m;
}
static void GetMaskName(int mask,WCHAR*buf,int sz){WCHAR p[3][16];int n=0;if(mask&1)wcscpy(p[n++],L"创建");if(mask&2)wcscpy(p[n++],L"修改");if(mask&4)wcscpy(p[n++],L"访问");if(!n){wcscpy(buf,L"无");return;}if(n==3){wcscpy(buf,L"全部");return;}buf[0]=0;for(int i=0;i<n;i++){if(i)wcscat(buf,L"+");wcscat(buf,p[i]);}}
static void SetDateTimeControls(HWND hDate, HWND hTime, const FILETIME *ft){
    FILETIME lf;SYSTEMTIME st;
    FileTimeToLocalFileTime(ft,&lf);FileTimeToSystemTime(&lf,&st);
    DateTime_SetSystemtime(hDate,GDT_VALID,&st);
    WCHAR tb[16];_snwprintf(tb,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
    SetWindowTextW(hTime,tb);
}
static BOOL GetDateTimeControls(HWND hDate, HWND hTime, FILETIME *ft){
    SYSTEMTIME st;WCHAR tb[16]={0};
    if(DateTime_GetSystemtime(hDate,&st)!=GDT_VALID)return FALSE;
    GetWindowTextW(hTime,tb,15);
    swscanf(tb,L"%hu:%hu:%hu",&st.wHour,&st.wMinute,&st.wSecond);
    st.wMilliseconds=0;
    FILETIME lf; if(!SystemTimeToFileTime(&st,&lf))return FALSE;
    return LocalFileTimeToFileTime(&lf,ft);
}
static LONGLONG ParseOffset(LPCWSTR s) { LONGLONG t=0;int sign=1;if(*s==L'-'){sign=-1;s++;}else if(*s==L'+')sign=1; LONGLONG num=0;int has=0; while(*s){if(*s>='0'&&*s<='9'){num=num*10+(*s-'0');s++;}else{LONGLONG m=0;switch(*s){case L'd':case L'D':m=864000000000LL;break;case L'h':case L'H':m=36000000000LL;break;case L'm':case L'M':m=600000000LL;break;case L's':case L'S':m=10000000LL;break;default:return 0;}t+=num*m;num=0;has=1;s++;}} if(num>0||!has)t+=num*10000000LL; return t*sign; }
static int DaysInMonth(int y,int m){static int d[]={31,28,31,30,31,30,31,31,30,31,30,31};if(m<1||m>12)return 30;int r=d[m-1];if(m==2&&((y%400==0)||(y%4==0&&y%100!=0)))r=29;return r;}

/* ===== 日期模式匹配（仅文件名用） ===== */
static int ParseChnYear(const WCHAR**pp){static const WCHAR*dg[]={L"〇零",L"一壹",L"二贰",L"三叁",L"四肆",L"五伍",L"六陆",L"七柒",L"八捌",L"九玖"};int v=0;for(int k=0;k<4;k++){int f=-1;for(int d=0;d<10;d++)if(wcschr(dg[d],(*pp)[k])){f=d;break;}if(f<0)return 0;v=v*10+f;}*pp+=4;return v;}
static int TryMatchDate(const WCHAR *text, const WCHAR *pos, SYSTEMTIME *st) {
    memset(st,0,sizeof(SYSTEMTIME));st->wDay=1; const WCHAR*p=pos;int y=0,m=0,d=0;
    if(p[0]>='0'&&p[0]<='9'&&p[1]>='0'&&p[1]<='9'&&p[2]>='0'&&p[2]<='9'&&p[3]>='0'&&p[3]<='9'){y=(p[0]-'0')*1000+(p[1]-'0')*100+(p[2]-'0')*10+(p[3]-'0');p+=4;}else if(wcschr(L"〇零一二三四五六七八九壹贰叁肆伍陆柒捌玖",p[0])){y=ParseChnYear(&p);}
    if(y<1900||y>2100)return 0;if(*p!=L'年')return 0;p++;
    if(*p>='0'&&*p<='9'){m=*p-'0';p++;if(*p>='0'&&*p<='9'){m=m*10+(*p-'0');p++;}}
    else{static const WCHAR*cm[]={L"",L"一",L"二",L"三",L"四",L"五",L"六",L"七",L"八",L"九",L"十",L"十一",L"十二"};int fn=0;for(int i=1;i<=12;i++){int ln=wcslen(cm[i]);if(wcsncmp(p,cm[i],ln)==0){m=i;p+=ln;fn=1;break;}}if(!fn)return 0;}
    if(m<1||m>12)return 0;if(*p!=L'月')return 0;p++;
    if(*p>='0'&&*p<='9'){d=*p-'0';p++;if(*p>='0'&&*p<='9'){d=d*10+(*p-'0');p++;}}if(*p==L'日')p++;else if(d==0)d=DaysInMonth(y,m);
    if(d<1||d>DaysInMonth(y,m))d=DaysInMonth(y,m);
    st->wYear=y;st->wMonth=m;st->wDay=d; return 1;
}
static int TryMatchDateNum(const WCHAR *text, const WCHAR *pos, SYSTEMTIME *st) {
    memset(st,0,sizeof(SYSTEMTIME));st->wDay=1;const WCHAR*p=pos;int y,m,d;
    if(p[0]<'0'||p[0]>'9'||p[1]<'0'||p[1]>'9'||p[2]<'0'||p[2]>'9'||p[3]<'0'||p[3]>'9')return 0;
    y=(p[0]-'0')*1000+(p[1]-'0')*100+(p[2]-'0')*10+(p[3]-'0');p+=4;if(y<1900||y>2100)return 0;
    WCHAR sep=*p;if(sep!=L'-'&&sep!=L'/'&&sep!=L'.')return 0;p++;
    if(p[0]<'0'||p[0]>'9')return 0;m=*p-'0';p++;if(*p>='0'&&*p<='9'){m=m*10+(*p-'0');p++;}if(m<1||m>12)return 0;
    if(*p!=sep)return 0;p++;
    if(p[0]<'0'||p[0]>'9'){d=DaysInMonth(y,m);}else{d=*p-'0';p++;if(*p>='0'&&*p<='9'){d=d*10+(*p-'0');p++;}if(d<1||d>31)d=DaysInMonth(y,m);}
    st->wYear=y;st->wMonth=m;st->wDay=d; return 1;
}

/* ===== 文件名转日期（直接填入，无弹窗） ===== */
#define MAX_FNAME_DATES 32
static int GetSelected(int **outIdx); /* forward */
static void DoFilenameDate(void)
{
    int *sel;int n=GetSelected(&sel);
    if(!n){
        if(g_fileCount>0){
            /* 未选中任何文件时默认选第一个 */
            ListView_SetCheckState(g_hListView,0,TRUE);
            n=1; sel=malloc(sizeof(int)); sel[0]=0;
        } else {
            MessageBoxW(g_hMainWnd,L"文件列表为空，请先添加文件！",L"提示",MB_ICONINFORMATION);return;
        }
    }
    int fidx=sel[0]; WCHAR *path=g_files[fidx].path; free(sel);

    /* 取纯文件名（去路径和扩展名） */
    WCHAR fname[MAX_PATH]; WCHAR *bs=wcsrchr(path,L'\\'); if(bs)wcscpy(fname,bs+1);else wcscpy(fname,path);
    WCHAR *dot=wcsrchr(fname,L'.'); if(dot)*dot=0;

    /* 扫描文件名中的日期 */
    SYSTEMTIME dates[MAX_FNAME_DATES]; int dc=0; LONGLONG bestNs=0; int bestIdx=-1;
    WCHAR*p=fname;
    while(*p&&dc<MAX_FNAME_DATES){
        while(*p&&!(*p>='0'&&*p<='9')&&!wcschr(L"〇零一二三四五六七八九壹贰叁肆伍陆柒捌玖",*p))p++;
        if(!*p)break;
        SYSTEMTIME st;
        if(TryMatchDate(fname,p,&st)||TryMatchDateNum(fname,p,&st)){
            LONGLONG ns=StToNs(&st);
            if(dc==0||ns>bestNs){ bestNs=ns; bestIdx=dc; }
            dates[dc++]=st;
        }
        p++;
    }
    if(bestIdx<0){ MessageBoxW(g_hMainWnd,L"在文件名中未找到可识别的日期。",L"提示",MB_ICONINFORMATION); return; }

    SYSTEMTIME st=dates[bestIdx];

    /* 时间部分: 参考文件修改时间，减随机300-900秒 */
    {
        FILETIME modFt=g_files[fidx].ftModify;
        FILETIME localFt; SYSTEMTIME modSt;
        FileTimeToLocalFileTime(&modFt,&localFt);
        FileTimeToSystemTime(&localFt,&modSt);

        if(modSt.wHour==0&&modSt.wMinute==0&&modSt.wSecond==0){
            /* 修改时间为0点，不调整 */
            st.wHour=0; st.wMinute=0; st.wSecond=0;
        } else {
            /* 用修改时间的时分秒减随机300~900秒 */
            long totalSec=modSt.wHour*3600+modSt.wMinute*60+modSt.wSecond;
            long rnd=300+(rand()%601); /* 300~900 */
            long newSec=totalSec-rnd;
            if(newSec<0) newSec=0; /* 不跨天 */
            st.wHour=(WORD)(newSec/3600);
            st.wMinute=(WORD)((newSec%3600)/60);
            st.wSecond=(WORD)(newSec%60);
        }
    }

    {
        FILETIME lf,ftM,ftC;
        SystemTimeToFileTime(&st,&lf);LocalFileTimeToFileTime(&lf,&ftM);
        LONGLONG rnd=6000000000LL+(rand()%3001)*10000000LL;
        Ns100ToFt(FtTo100ns(&ftM)-rnd,&ftC);
        SetDateTimeControls(g_hCDatePicker,g_hCTimeEdit,&ftC);
        SetDateTimeControls(g_hMDatePicker,g_hMTimeEdit,&ftM);
        SetDateTimeControls(g_hADatePicker,g_hATimeEdit,&ftM);
    }

    WCHAR ms[256];
    _snwprintf(ms,256,L"从文件名 [%s] 提取日期 %04d-%02d-%02d %02d:%02d:%02d",
        fname, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    SetStatus(ms);
}

/* ===== 列表 ===== */
static void RefreshList(void) {
    ListView_DeleteAllItems(g_hListView); RECT rc;GetClientRect(g_hListView,&rc);
    int tw=rc.right-rc.left-GetSystemMetrics(SM_CXVSCROLL)-4, cw[]={tw*38/100,tw*20/100,tw*21/100,tw*21/100};
    LVCOLUMNW lc={LVCF_WIDTH}; for(int i=0;i<4;i++){lc.cx=cw[i];ListView_SetColumn(g_hListView,i,&lc);}
    LVITEMW li={LVIF_TEXT};
    for(int i=0;i<g_fileCount;i++){WCHAR ct[32],mt[32],at[32];FtToStr(&g_files[i].ftCreate,ct,32);FtToStr(&g_files[i].ftModify,mt,32);FtToStr(&g_files[i].ftAccess,at,32);li.iItem=i;li.iSubItem=0;li.pszText=g_files[i].path;ListView_InsertItem(g_hListView,&li);ListView_SetItemText(g_hListView,i,1,ct);ListView_SetItemText(g_hListView,i,2,mt);ListView_SetItemText(g_hListView,i,3,at);}
    WCHAR st[128];_snwprintf(st,128,L"共 %d 个文件",g_fileCount);SetStatus(st);
}

/* 递归扫描目录: level=0 仅本目录，level>0 包含子目录, maxDepth 防止过深 */
static void ScanDirRecursive(const WCHAR *base, int dirLen, int maxDepth, int *added){
    if(maxDepth<0||g_fileCount>=MAX_FILES)return;
    WCHAR pattern[MAX_PATH];
    int n=_snwprintf(pattern,MAX_PATH,L"%s\\*",base);
    if(n<0||n>=MAX_PATH)return;

    WIN32_FIND_DATAW fd;
    HANDLE fh=FindFirstFileW(pattern,&fd);
    if(fh==INVALID_HANDLE_VALUE)return;

    do{
        if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0)continue;
        if(g_fileCount>=MAX_FILES)break;

        /* 拼接完整路径 */
        WCHAR full[MAX_PATH];
        int fl=_snwprintf(full,MAX_PATH,L"%s\\%s",base,fd.cFileName);
        if(fl<0||fl>=MAX_PATH)continue;

        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            /* 递归下级目录 */
            ScanDirRecursive(full,fl,maxDepth-1,added);
        } else {
            /* 去重检查 */
            BOOL dup=FALSE;
            for(int j=0;j<g_fileCount;j++)
                if(_wcsicmp(g_files[j].path,full)==0){dup=TRUE;break;}
            if(dup)continue;

            wcscpy(g_files[g_fileCount].path,full);
            if(FileGetTimes(full,&g_files[g_fileCount].ftCreate,
                            &g_files[g_fileCount].ftModify,
                            &g_files[g_fileCount].ftAccess)){
                g_fileCount++;
                (*added)++;
            }
        }
    }while(FindNextFileW(fh,&fd));
    FindClose(fh);
}

/* 拖拽/对话框添加入口: dir 形如 "C:\xx" 或 "C:\xx\sub" */
static void ScanDirectoryTree(const WCHAR *dir, int *added){
    DWORD attr=GetFileAttributesW(dir);
    if(attr==INVALID_FILE_ATTRIBUTES||!(attr&FILE_ATTRIBUTE_DIRECTORY))return;
    int len=(int)wcslen(dir);
    while(len>0&&dir[len-1]==L'\\')len--;  /* 去尾随 \\ */
    if(g_hChkRecursive && SendMessageW(g_hChkRecursive,BM_GETCHECK,0,0)==BST_CHECKED){
        ScanDirRecursive(dir,len,16,added);  /* 递归扫描 */
    } else {
        /* 仅本目录 */
        WCHAR pattern[MAX_PATH];
        _snwprintf(pattern,MAX_PATH,L"%s\\*",dir);
        WIN32_FIND_DATAW fd;
        HANDLE fh=FindFirstFileW(pattern,&fd);
        if(fh!=INVALID_HANDLE_VALUE){
            do{
                if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0)continue;
                if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
                if(g_fileCount>=MAX_FILES)break;
                WCHAR full[MAX_PATH];
                _snwprintf(full,MAX_PATH,L"%s\\%s",dir,fd.cFileName);
                BOOL dup=FALSE;
                for(int j=0;j<g_fileCount;j++)if(_wcsicmp(g_files[j].path,full)==0){dup=TRUE;break;}
                if(dup)continue;
                wcscpy(g_files[g_fileCount].path,full);
                if(FileGetTimes(full,&g_files[g_fileCount].ftCreate,&g_files[g_fileCount].ftModify,&g_files[g_fileCount].ftAccess)){
                    g_fileCount++;(*added)++;
                }
            }while(FindNextFileW(fh,&fd));
            FindClose(fh);
        }
    }
}

static void AddPaths(WCHAR**paths,int count){
    int added=0;
    for(int i=0;i<count&&g_fileCount<MAX_FILES;i++){
        DWORD attr=GetFileAttributesW(paths[i]);
        if(attr==INVALID_FILE_ATTRIBUTES)continue;
        if(attr&FILE_ATTRIBUTE_DIRECTORY){
            /* 文件夹: 递归扫描全部子目录 */
            ScanDirectoryTree(paths[i],&added);
        } else {
            /* 单文件: 原有逻辑 */
            BOOL dup=FALSE;
            for(int j=0;j<g_fileCount;j++)
                if(_wcsicmp(g_files[j].path,paths[i])==0){dup=TRUE;break;}
            if(dup)continue;
            wcscpy(g_files[g_fileCount].path,paths[i]);
            if(FileGetTimes(paths[i],&g_files[g_fileCount].ftCreate,
                            &g_files[g_fileCount].ftModify,
                            &g_files[g_fileCount].ftAccess)){
                g_fileCount++;
                added++;
            }
        }
    }
    RefreshList();
    if(added>0){
        WCHAR ms[128];
        _snwprintf(ms,128,L"已添加 %d 个文件 (共 %d)",added,g_fileCount);
        SetStatus(ms);
    }
}
static int GetSelected(int**outIdx){
    int cnt=0; for(int i=0;i<g_fileCount;i++){if(ListView_GetCheckState(g_hListView,i)||(ListView_GetItemState(g_hListView,i,LVIS_SELECTED)&LVIS_SELECTED))cnt++;}
    if(cnt<=0){*outIdx=NULL;return 0;}int*idx=malloc(cnt*sizeof(int));if(!idx){*outIdx=NULL;return 0;}
    int n=0;for(int i=0;i<g_fileCount;i++){if(ListView_GetCheckState(g_hListView,i)||(ListView_GetItemState(g_hListView,i,LVIS_SELECTED)&LVIS_SELECTED))idx[n++]=i;}
    *outIdx=idx;return n;
}
static void RemoveFileAt(int idx) { if(idx<0||idx>=g_fileCount)return; for(int i=idx;i<g_fileCount-1;i++)g_files[i]=g_files[i+1];g_fileCount--;RefreshList(); }

/* ===== 文件编辑对话框 ===== */
static HWND g_hEditDlg=NULL, g_hEdCDate, g_hEdCTime, g_hEdMDate, g_hEdMTime, g_hEdADate, g_hEdATime;
static HWND g_hEdCChk, g_hEdMChk, g_hEdAChk;
static int  g_edIndex=-1;
static BOOL CALLBACK FontCallback(HWND h,LPARAM lp); /* forward */

static void OpenEditDialog(int idx);
static LRESULT CALLBACK EditDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

static void OpenEditDialog(int idx){
    if(idx<0||idx>=g_fileCount)return;
    g_edIndex=idx;

    /* 先在屏幕上居中计算坐标 */
    RECT rcParent;GetWindowRect(g_hMainWnd,&rcParent);
    int x=rcParent.left+50, y=rcParent.top+50;

    /* 这是一个带父窗口的顶层弹窗，不带 WS_CHILD。
       因此 hMenu 参数必须传 NULL；如果传控制 ID，会被系统当成菜单句柄，
       CreateWindowExW 会返回 ERROR_INVALID_MENU_HANDLE(1401)。 */
    g_hEditDlg=CreateWindowExW(0,L"FtoolEditDlg",L"编辑文件时间",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
        x,y,420,300,
        g_hMainWnd,NULL,g_hInst,NULL);
    if(!g_hEditDlg){
        WCHAR er[64];_snwprintf(er,64,L"CreateWindow failed, err=%lu",GetLastError());
        SetStatus(er);
        return;
    }
    WCHAR ok[64];_snwprintf(ok,64,L"EditDlg created hwnd=%p",(void*)g_hEditDlg);
    SetStatus(ok);

    /* 居中显示 */
    RECT rc;GetWindowRect(g_hEditDlg,&rc);
    int w=rc.right-rc.left, h=rc.bottom-rc.top;
    int nx=rcParent.left+(rcParent.right-rcParent.left-w)/2;
    int ny=rcParent.top+(rcParent.bottom-rcParent.top-h)/2;
    SetWindowPos(g_hEditDlg,NULL,nx,ny,0,0,SWP_NOSIZE|SWP_NOZORDER);

    /* 顶部显示文件路径 */
    HWND hPath=CreateWindowW(L"STATIC",g_files[idx].path,WS_CHILD|WS_VISIBLE|SS_PATHELLIPSIS|WS_CLIPSIBLINGS,
        10,10,380,18,g_hEditDlg,NULL,g_hInst,NULL);

    /* 时间属性组 */
    int y0=42;
    HFONT hf=(HFONT)SendMessageW(g_hListView,WM_GETFONT,0,0);

    const WCHAR*labels[]={L"创建时间:", L"修改时间:", L"访问时间:"};
    HWND *pDate[]={&g_hEdCDate,&g_hEdMDate,&g_hEdADate};
    HWND *pTime[]={&g_hEdCTime,&g_hEdMTime,&g_hEdATime};
    HWND *pChk[]={&g_hEdCChk,&g_hEdMChk,&g_hEdAChk};
    const FILETIME *pFt[]={&g_files[idx].ftCreate,&g_files[idx].ftModify,&g_files[idx].ftAccess};

    for(int i=0;i<3;i++){
        *pChk[i]=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_CLIPSIBLINGS,
            10,y0+i*36,80,22,g_hEditDlg,(HMENU)(INT_PTR)(ID_ED_CCHK+i),g_hInst,NULL);
        SendMessageW(*pChk[i],WM_SETFONT,(WPARAM)hf,TRUE);
        SendMessageW(*pChk[i],BM_SETCHECK,BST_CHECKED,0);

        *pDate[i]=CreateWindowExW(0,DATETIMEPICK_CLASSW,L"",WS_CHILD|WS_VISIBLE|DTS_SHORTDATEFORMAT|WS_CLIPSIBLINGS,
            95,y0+i*36,110,22,g_hEditDlg,(HMENU)(INT_PTR)(ID_ED_CDATE+i*2),g_hInst,NULL);
        SendMessageW(*pDate[i],WM_SETFONT,(WPARAM)hf,TRUE);
        {HWND hCal=(HWND)SendMessageW(*pDate[i],DTM_GETMONTHCAL,0,0);if(hCal)SendMessageW(hCal,MCM_SETFIRSTDAYOFWEEK,0,0);}

        *pTime[i]=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_CENTER|WS_CLIPSIBLINGS,
            215,y0+i*36+1,70,22,g_hEditDlg,(HMENU)(INT_PTR)(ID_ED_CTIME+i),g_hInst,NULL);
        SendMessageW(*pTime[i],WM_SETFONT,(WPARAM)hf,TRUE);

        /* 默认值: 文件当前时间 */
        FILETIME lf;SYSTEMTIME st;
        FileTimeToLocalFileTime(pFt[i],&lf);FileTimeToSystemTime(&lf,&st);
        DateTime_SetSystemtime(*pDate[i],GDT_VALID,&st);
        WCHAR tb[16];_snwprintf(tb,16,L"%02d:%02d:%02d",st.wHour,st.wMinute,st.wSecond);
        SetWindowTextW(*pTime[i],tb);
    }

    /* 竖向“时间一致”按钮（覆盖三行） */
    CreateWindowW(L"BUTTON",L"时\n间\n一\n致",WS_CHILD|WS_VISIBLE|BS_MULTILINE|WS_CLIPSIBLINGS,
        295,y0,24,110,g_hEditDlg,(HMENU)(INT_PTR)ID_ED_SYNC,g_hInst,NULL);

    /* 按钮 */
    int yBtn=160;
    CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON|WS_CLIPSIBLINGS,
        220,yBtn,80,28,g_hEditDlg,(HMENU)(INT_PTR)ID_ED_OK,g_hInst,NULL);
    CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
        310,yBtn,80,28,g_hEditDlg,(HMENU)(INT_PTR)ID_ED_CANCEL,g_hInst,NULL);

    EnumChildWindows(g_hEditDlg,FontCallback,(LPARAM)hf);
    EnableWindow(g_hMainWnd,FALSE);
    ShowWindow(g_hEditDlg,SW_SHOW);
    UpdateWindow(g_hEditDlg);
}

static LRESULT CALLBACK EditDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp){
    switch(msg){
    case WM_COMMAND:switch(LOWORD(wp)){
        case ID_ED_CANCEL:
        case IDCANCEL:
            DestroyWindow(hDlg);return 0;
        case ID_ED_SYNC:{
            /* 读取修改时间为基准 */
            SYSTEMTIME stM; WCHAR tbM[16]={0};
            DateTime_GetSystemtime(g_hEdMDate,&stM);
            GetWindowTextW(g_hEdMTime,tbM,15);
            swscanf(tbM,L"%hu:%hu:%hu",&stM.wHour,&stM.wMinute,&stM.wSecond);
            FILETIME lf,utc; SystemTimeToFileTime(&stM,&lf); LocalFileTimeToFileTime(&lf,&utc);
            if(SendMessageW(g_hEdCChk,BM_GETCHECK,0,0)==BST_CHECKED){
                DateTime_SetSystemtime(g_hEdCDate,GDT_VALID,&stM);
                SetWindowTextW(g_hEdCTime,tbM);
            }
            if(SendMessageW(g_hEdAChk,BM_GETCHECK,0,0)==BST_CHECKED){
                DateTime_SetSystemtime(g_hEdADate,GDT_VALID,&stM);
                SetWindowTextW(g_hEdATime,tbM);
            }
            return 0;
        }
        case ID_ED_OK:{
            /* 收集三个时间 */
            SYSTEMTIME stC,stM,stA;
            WCHAR tbC[16]={0},tbM[16]={0},tbA[16]={0};
            BOOL cOn=(SendMessageW(g_hEdCChk,BM_GETCHECK,0,0)==BST_CHECKED);
            BOOL mOn=(SendMessageW(g_hEdMChk,BM_GETCHECK,0,0)==BST_CHECKED);
            BOOL aOn=(SendMessageW(g_hEdAChk,BM_GETCHECK,0,0)==BST_CHECKED);
            if(!cOn&&!mOn&&!aOn){MessageBoxW(hDlg,L"请至少勾选一项要修改的时间！",L"提示",MB_ICONINFORMATION);return 0;}

            FILETIME ftC,ftM,ftA;
            int mask=0;
            if(cOn){
                DateTime_GetSystemtime(g_hEdCDate,&stC);
                GetWindowTextW(g_hEdCTime,tbC,15);
                swscanf(tbC,L"%hu:%hu:%hu",&stC.wHour,&stC.wMinute,&stC.wSecond);
                FILETIME lf,utc;SystemTimeToFileTime(&stC,&lf);LocalFileTimeToFileTime(&lf,&utc);ftC=utc;
                mask|=1;
            }
            if(mOn){
                DateTime_GetSystemtime(g_hEdMDate,&stM);
                GetWindowTextW(g_hEdMTime,tbM,15);
                swscanf(tbM,L"%hu:%hu:%hu",&stM.wHour,&stM.wMinute,&stM.wSecond);
                FILETIME lf,utc;SystemTimeToFileTime(&stM,&lf);LocalFileTimeToFileTime(&lf,&utc);ftM=utc;
                mask|=2;
            }
            if(aOn){
                DateTime_GetSystemtime(g_hEdADate,&stA);
                GetWindowTextW(g_hEdATime,tbA,15);
                swscanf(tbA,L"%hu:%hu:%hu",&stA.wHour,&stA.wMinute,&stA.wSecond);
                FILETIME lf,utc;SystemTimeToFileTime(&stA,&lf);LocalFileTimeToFileTime(&lf,&utc);ftA=utc;
                mask|=4;
            }

            if(g_edIndex<0||g_edIndex>=g_fileCount){DestroyWindow(hDlg);return 0;}
            if(FileSetTimes(g_files[g_edIndex].path,
                            cOn?&ftC:NULL,
                            mOn?&ftM:NULL,
                            aOn?&ftA:NULL,
                            mask)){
                if(cOn)g_files[g_edIndex].ftCreate=ftC;
                if(mOn)g_files[g_edIndex].ftModify=ftM;
                if(aOn)g_files[g_edIndex].ftAccess=ftA;
                WCHAR ms[128];
                _snwprintf(ms,128,L"已修改文件时间: %s",g_files[g_edIndex].path);
                SetStatus(ms);
                RefreshList();
            } else {
                MessageBoxW(hDlg,L"修改失败！文件可能正在被占用或没有权限。",L"错误",MB_ICONERROR);
                return 0;
            }
            DestroyWindow(hDlg);
            return 0;
        }
    }break;
    case WM_CLOSE:DestroyWindow(hDlg);return 0;
    case WM_DESTROY:
        EnableWindow(g_hMainWnd,TRUE);SetForegroundWindow(g_hMainWnd);
        g_hEditDlg=NULL;g_edIndex=-1;
        return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{HDC hdc=(HDC)wp;SetBkColor(hdc,GetSysColor(COLOR_BTNFACE));return(LRESULT)g_hBgBrush;}
    }return DefWindowProcW(hDlg,msg,wp,lp);
}

/* ===== 时间操作 ===== */
/* 保持日期不变，随机化时分秒 (7:00~22:59) */
static FILETIME RandomTimeKeepDate(const FILETIME*ft){
    FILETIME lf,utc; SYSTEMTIME st;
    FileTimeToLocalFileTime(ft,&lf);FileTimeToSystemTime(&lf,&st);
    st.wHour=(WORD)(rand()%16+7);st.wMinute=(WORD)(rand()%60);st.wSecond=(WORD)(rand()%60);st.wMilliseconds=0;
    SystemTimeToFileTime(&st,&lf);LocalFileTimeToFileTime(&lf,&utc);
    return utc;
}
static void ApplyTime(FILETIME*pC,FILETIME*pM,FILETIME*pA){
    int mask=GetTimeMask();if(!mask){MessageBoxW(g_hMainWnd,L"请至少勾选一项时间类型！",L"提示",MB_ICONWARNING);return;}
    int*sel;int n=GetSelected(&sel);if(!n){MessageBoxW(g_hMainWnd,L"请先在列表中选择文件！",L"提示",MB_ICONINFORMATION);return;}
    int ok=0,fail=0;
    for(int i=0;i<n;i++){int idx=sel[i];
        FILETIME*c=(mask&1)?pC:&g_files[idx].ftCreate;
        FILETIME*m=(mask&2)?pM:&g_files[idx].ftModify;
        FILETIME*a=(mask&4)?pA:&g_files[idx].ftAccess;
        if(FileSetTimes(g_files[idx].path,c,m,a,mask)){
            if(mask&1)g_files[idx].ftCreate=*c;if(mask&2)g_files[idx].ftModify=*m;if(mask&4)g_files[idx].ftAccess=*a;ok++;
        }else fail++;
    }free(sel);RefreshList();WCHAR mn[32],ms[128];GetMaskName(mask,mn,32);_snwprintf(ms,128,L"[%s] 完成: 成功 %d, 失败 %d",mn,ok,fail);SetStatus(ms);
}
static void AdjustSelected(LONGLONG offset){
    int mask=GetTimeMask();if(!mask){MessageBoxW(g_hMainWnd,L"请至少勾选一项时间类型！",L"提示",MB_ICONWARNING);return;}
    int*sel;int n=GetSelected(&sel);if(!n){MessageBoxW(g_hMainWnd,L"请先在列表中选择文件！",L"提示",MB_ICONINFORMATION);return;}
    int both=((mask&(MASK_CREATE|MASK_MODIFY))==(MASK_CREATE|MASK_MODIFY));
    int ok=0,fail=0;
    for(int i=0;i<n;i++){int idx=sel[i];FILETIME nc,nm,na;
        Ns100ToFt(FtTo100ns(&g_files[idx].ftModify)+offset,&nm);
        if(both){
            LONGLONG rnd=6000000000LL+(rand()%3001)*10000000LL;
            Ns100ToFt(FtTo100ns(&nm)-rnd,&nc);
        } else {
            Ns100ToFt(FtTo100ns(&g_files[idx].ftCreate)+offset,&nc);
        }
        Ns100ToFt(FtTo100ns(&g_files[idx].ftAccess)+offset,&na);
        if(FileSetTimes(g_files[idx].path,&nc,&nm,&na,mask)){
            if(mask&1)g_files[idx].ftCreate=nc;if(mask&2)g_files[idx].ftModify=nm;if(mask&4)g_files[idx].ftAccess=na;ok++;
        }else fail++;
    }free(sel);RefreshList();WCHAR mn[32],ms[128];GetMaskName(mask,mn,32);_snwprintf(ms,128,L"[%s] 偏移 %+lld 秒: 成功 %d, 失败 %d",mn,offset/10000000LL,ok,fail);SetStatus(ms);
}
static void DoCustom(void){WCHAR buf[128]={0};GetWindowTextW(g_hCustomEdit,buf,127);if(!buf[0]){MessageBoxW(g_hMainWnd,L"请输入偏移量!",L"提示",MB_ICONINFORMATION);return;}LONGLONG off=ParseOffset(buf);if(!off&&wcscmp(buf,L"0")){MessageBoxW(g_hMainWnd,L"格式错误！\n支持: -1d, +2h, -30m, +10s",L"提示",MB_ICONWARNING);return;}AdjustSelected(off);}
/* 随机时间：在给定FILETIME的日期基础上，随机时分秒 [minH, maxH] 范围 */
static FILETIME RandomTimeInRange(const FILETIME*ft, int minH, int maxH, int maxM, int maxS){
    FILETIME lf,utc; SYSTEMTIME st;
    FileTimeToLocalFileTime(ft,&lf);FileTimeToSystemTime(&lf,&st);
    if(minH<0)minH=0; if(maxH>23)maxH=23; if(minH>maxH){int t=minH;minH=maxH;maxH=t;}
    int h=minH+rand()%(maxH-minH+1);
    int m,s;
    if(h==maxH){ m=rand()%(maxM+1); s=(m==maxM&&maxS>=0)?rand()%(maxS+1):rand()%60; }
    else { m=rand()%60; s=rand()%60; }
    st.wHour=(WORD)h;st.wMinute=(WORD)m;st.wSecond=(WORD)s;st.wMilliseconds=0;
    SystemTimeToFileTime(&st,&lf);LocalFileTimeToFileTime(&lf,&utc);
    return utc;
}
static int TimeInRangeDup(LONGLONG*times, int i, LONGLONG v){
    for(int j=0;j<i;j++)if(times[j]==v)return 1;
    return 0;
}
/* 随机时间：多个文件时，保持日期不变，时分秒随机7-22点且各文件不同 */
static void DoRandomTime(void){
    int mask=GetTimeMask();if(!mask){MessageBoxW(g_hMainWnd,L"请至少勾选一项时间类型！",L"提示",MB_ICONWARNING);return;}
    int*sel;int n=GetSelected(&sel);if(!n){MessageBoxW(g_hMainWnd,L"请先在列表中选择文件！",L"提示",MB_ICONINFORMATION);return;}
    int both=((mask&(MASK_CREATE|MASK_MODIFY))==(MASK_CREATE|MASK_MODIFY));

    SYSTEMTIME targetDate; DateTime_GetSystemtime(g_hRandDatePicker,&targetDate);
    FILETIME lf,dateFt; SystemTimeToFileTime(&targetDate,&lf); LocalFileTimeToFileTime(&lf,&dateFt);

    SYSTEMTIME now; GetLocalTime(&now);
    int isToday=(targetDate.wYear==now.wYear&&targetDate.wMonth==now.wMonth&&targetDate.wDay==now.wDay);
    int minH=7, maxH=23, maxM=59, maxS=59;
    WCHAR tag[32]=L"";
    if(isToday){
        if(now.wHour==0&&now.wMinute==0){
            /* 午夜0点，无"之前"的概念，直接用7-22 */
            wcscpy(tag,L"(今日,午夜)");
        } else if(now.wHour>7){
            maxH=now.wHour; maxM=now.wMinute; maxS=now.wSecond;
            wcscpy(tag,L"(今日,早于当前)");
        } else {
            /* 当前小于7点，落在[当前-2h, 当前] */
            int pastSec=now.wHour*3600+now.wMinute*60+now.wSecond;
            int twoH=7200;
            int startSec=pastSec-twoH; if(startSec<0)startSec=0;
            minH=startSec/3600; maxH=now.wHour; maxM=now.wMinute; maxS=now.wSecond;
            wcscpy(tag,L"(今日,近2小时)");
        }
    }

    LONGLONG *times=malloc(n*sizeof(LONGLONG));
    if(!times){free(sel);return;}
    for(int i=0;i<n;i++){
        FILETIME ft;
        int tries=0;
        do{
            ft=RandomTimeInRange(&dateFt,minH,maxH,maxM,maxS);
            tries++;
        }while(tries<100&&TimeInRangeDup(times,i,FtTo100ns(&ft)));
        times[i]=FtTo100ns(&ft);
    }

    int ok=0,fail=0;
    for(int i=0;i<n;i++){int idx=sel[i];
        FILETIME nm; Ns100ToFt(times[i],&nm);
        FILETIME nc; if(both){LONGLONG rnd=6000000000LL+(rand()%3001)*10000000LL;Ns100ToFt(times[i]-rnd,&nc);}else{nc=nm;}
        FILETIME na=nm;
        if(FileSetTimes(g_files[idx].path,&nc,&nm,&na,mask)){
            if(mask&1)g_files[idx].ftCreate=nc;if(mask&2)g_files[idx].ftModify=nm;if(mask&4)g_files[idx].ftAccess=na;ok++;
        }else fail++;
    }
    free(times);free(sel);RefreshList();WCHAR mn[32],ms[128];GetMaskName(mask,mn,32);_snwprintf(ms,128,L"[%s] 随机时间%s 成功%d 失败%d",mn,tag,ok,fail);SetStatus(ms);
}
static BOOL CALLBACK FontCallback(HWND h,LPARAM lp){SendMessageW(h,WM_SETFONT,(WPARAM)lp,TRUE);return TRUE;}

/* ===== 窗口过程 ===== */
static LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        g_hMainWnd=hWnd; g_hBgBrush=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        NONCLIENTMETRICSW nm={sizeof(nm)};SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(nm),&nm,0);g_hFont=CreateFontIndirectW(&nm.lfMessageFont);
        g_hStatusBar=CreateWindowExW(0,STATUSCLASSNAMEW,L"就绪",WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,0,0,0,0,hWnd,(HMENU)ID_STATUSBAR,g_hInst,NULL);

        int bH=27;
        CreateWindowW(L"BUTTON",L"+ 添加文件夹",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,8,6,95,bH,hWnd,(HMENU)ID_BTN_ADDDIR,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"+ 添加文件",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,108,6,95,bH,hWnd,(HMENU)ID_BTN_ADD,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"x 清空",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,208,6,55,bH,hWnd,(HMENU)ID_BTN_CLEAR,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"全选文件",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,272,6,70,bH,hWnd,(HMENU)ID_FILE_SELALL,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"反选文件",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,346,6,70,bH,hWnd,(HMENU)ID_FILE_INVERT,g_hInst,NULL);
        g_hChkRecursive=CreateWindowW(L"BUTTON",L"包含子目录",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_CLIPSIBLINGS,420,6,90,bH,hWnd,(HMENU)ID_CHK_RECURSIVE,g_hInst,NULL);
        SendMessageW(g_hChkRecursive,BM_SETCHECK,BST_CHECKED,0); /* 默认勾选 */

        g_hListView=CreateWindowExW(WS_EX_CLIENTEDGE,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS|WS_CLIPSIBLINGS,8,38,680,192,hWnd,(HMENU)ID_LISTVIEW,g_hInst,NULL);
        ListView_SetExtendedListViewStyle(g_hListView,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES|LVS_EX_CHECKBOXES);
        LVCOLUMNW lc={LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM};WCHAR*cols[]={L"文件名",L"创建时间",L"修改时间",L"访问时间"};int cw[]={260,135,135,135};
        for(int i=0;i<4;i++){lc.pszText=cols[i];lc.cx=cw[i];lc.iSubItem=i;ListView_InsertColumn(g_hListView,i,&lc);}

        int yChk=238;
        g_hChkCreate=CreateWindowW(L"BUTTON",L"创建时间:",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_CLIPSIBLINGS,34,yChk,92,22,hWnd,(HMENU)ID_CHK_CREATE,g_hInst,NULL);SendMessageW(g_hChkCreate,BM_SETCHECK,BST_CHECKED,0);
        g_hCDatePicker=CreateWindowExW(0,DATETIMEPICK_CLASSW,L"",WS_CHILD|WS_VISIBLE|DTS_SHORTDATEFORMAT|WS_CLIPSIBLINGS,132,yChk-1,130,bH,hWnd,(HMENU)ID_C_DATEPICKER,g_hInst,NULL);
        g_hCTimeEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_CENTER|WS_CLIPSIBLINGS,274,yChk+1,78,22,hWnd,(HMENU)ID_C_TIMEEDIT,g_hInst,NULL);

        g_hChkModify=CreateWindowW(L"BUTTON",L"修改时间:",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_CLIPSIBLINGS,34,yChk+34,92,22,hWnd,(HMENU)ID_CHK_MODIFY,g_hInst,NULL);SendMessageW(g_hChkModify,BM_SETCHECK,BST_CHECKED,0);
        g_hMDatePicker=CreateWindowExW(0,DATETIMEPICK_CLASSW,L"",WS_CHILD|WS_VISIBLE|DTS_SHORTDATEFORMAT|WS_CLIPSIBLINGS,132,yChk+33,130,bH,hWnd,(HMENU)ID_M_DATEPICKER,g_hInst,NULL);
        g_hMTimeEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_CENTER|WS_CLIPSIBLINGS,274,yChk+35,78,22,hWnd,(HMENU)ID_M_TIMEEDIT,g_hInst,NULL);

        g_hChkAccess=CreateWindowW(L"BUTTON",L"访问时间:",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX|WS_CLIPSIBLINGS,34,yChk+68,92,22,hWnd,(HMENU)ID_CHK_ACCESS,g_hInst,NULL);
        g_hADatePicker=CreateWindowExW(0,DATETIMEPICK_CLASSW,L"",WS_CHILD|WS_VISIBLE|DTS_SHORTDATEFORMAT|WS_CLIPSIBLINGS,132,yChk+67,130,bH,hWnd,(HMENU)ID_A_DATEPICKER,g_hInst,NULL);
        g_hATimeEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_CENTER|WS_CLIPSIBLINGS,274,yChk+69,78,22,hWnd,(HMENU)ID_A_TIMEEDIT,g_hInst,NULL);

        g_hDatePicker=g_hMDatePicker;
        g_hTimeEdit=g_hMTimeEdit;
        {SYSTEMTIME st;GetLocalTime(&st);FILETIME lf,ft;SystemTimeToFileTime(&st,&lf);LocalFileTimeToFileTime(&lf,&ft);
            SetDateTimeControls(g_hCDatePicker,g_hCTimeEdit,&ft);
            SetDateTimeControls(g_hMDatePicker,g_hMTimeEdit,&ft);
            SetDateTimeControls(g_hADatePicker,g_hATimeEdit,&ft);
            HWND hCal=(HWND)SendMessageW(g_hCDatePicker,DTM_GETMONTHCAL,0,0); if(hCal)SendMessageW(hCal,MCM_SETFIRSTDAYOFWEEK,0,0);
            hCal=(HWND)SendMessageW(g_hMDatePicker,DTM_GETMONTHCAL,0,0); if(hCal)SendMessageW(hCal,MCM_SETFIRSTDAYOFWEEK,0,0);
            hCal=(HWND)SendMessageW(g_hADatePicker,DTM_GETMONTHCAL,0,0); if(hCal)SendMessageW(hCal,MCM_SETFIRSTDAYOFWEEK,0,0);
        }
        /* 竖向“时间一致”按钮（覆盖三行） */
        CreateWindowW(L"BUTTON",L"时\n间\n一\n致",WS_CHILD|WS_VISIBLE|BS_MULTILINE|WS_CLIPSIBLINGS,
            360,yChk,24,110,hWnd,(HMENU)ID_BTN_SYNCMAIN,g_hInst,NULL);

        CreateWindowW(L"BUTTON",L"应用指定时间",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|BS_DEFPUSHBUTTON,390,yChk-1,110,bH,hWnd,(HMENU)ID_BTN_SETSPEC,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"重置为现在",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,390,yChk+33,90,bH,hWnd,(HMENU)ID_BTN_SETNOW,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取文件名时间",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,390,yChk+67,100,bH,hWnd,(HMENU)ID_BTN_FILEDATE,g_hInst,NULL);

        int y2=346;
        CreateWindowW(L"STATIC",L"快捷:",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,10,y2+5,34,20,hWnd,NULL,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"-1天",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,46,y2,50,bH,hWnd,(HMENU)ID_BTN_MINUS1D,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"-1时",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,100,y2,50,bH,hWnd,(HMENU)ID_BTN_MINUS1H,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"+1时",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,154,y2,50,bH,hWnd,(HMENU)ID_BTN_PLUS1H,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"+1天",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,208,y2,50,bH,hWnd,(HMENU)ID_BTN_PLUS1D,g_hInst,NULL);
        CreateWindowW(L"STATIC",L"自定义:",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,268,y2+5,48,20,hWnd,NULL,g_hInst,NULL);
        g_hCustomEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"-1d",WS_CHILD|WS_VISIBLE|ES_CENTER|WS_CLIPSIBLINGS,318,y2+2,90,22,hWnd,(HMENU)ID_CUSTOM_EDIT,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"执行",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,414,y2,50,bH,hWnd,(HMENU)ID_BTN_CUSTOM,g_hInst,NULL);

        /* 随机时间行 */
        int y3=380;
        CreateWindowW(L"STATIC",L"随机日期:",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,10,y3+5,58,20,hWnd,NULL,g_hInst,NULL);
        g_hRandDatePicker=CreateWindowExW(0,DATETIMEPICK_CLASSW,L"",WS_CHILD|WS_VISIBLE|DTS_SHORTDATEFORMAT|WS_CLIPSIBLINGS,70,y3,130,bH,hWnd,(HMENU)ID_DATEPICKER2,g_hInst,NULL);
        {SYSTEMTIME st;GetLocalTime(&st);DateTime_SetSystemtime(g_hRandDatePicker,GDT_VALID,&st);SendMessageW(g_hRandDatePicker,WM_SETFONT,(WPARAM)g_hFont,TRUE);}
        { HWND hCal=(HWND)SendMessageW(g_hRandDatePicker,DTM_GETMONTHCAL,0,0); if(hCal)SendMessageW(hCal,MCM_SETFIRSTDAYOFWEEK,0,0); }
        CreateWindowW(L"BUTTON",L"随机时间",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|BS_DEFPUSHBUTTON,210,y3,90,bH,hWnd,(HMENU)ID_BTN_RANDTIME,g_hInst,NULL);

        CreateWindowW(L"BUTTON",L"全选并执行以上调整",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,8,414,180,30,hWnd,(HMENU)ID_BTN_SELALL,g_hInst,NULL);

        EnumChildWindows(hWnd,FontCallback,(LPARAM)g_hFont); DragAcceptFiles(hWnd,TRUE);
        SetStatus(L"就绪 - 拖放文件或点击 [+ 添加文件]  右键列表项可移除");
        return 0;
    }
    case WM_SIZE:{SendMessageW(g_hStatusBar,WM_SIZE,0,0);RECT rc;GetClientRect(hWnd,&rc);int sbH=0;{RECT sr;GetWindowRect(g_hStatusBar,&sr);sbH=sr.bottom-sr.top;}if(g_hListView){int lvH=rc.bottom-rc.top-sbH-260;if(lvH<80)lvH=80;SetWindowPos(g_hListView,NULL,8,38,rc.right-rc.left-16,lvH,SWP_NOZORDER);}return 0;}
    case WM_DROPFILES:{HDROP hd=(HDROP)wp;UINT n=DragQueryFileW(hd,0xFFFFFFFF,NULL,0);WCHAR**pp=malloc(n*sizeof(WCHAR*));if(pp){for(UINT i=0;i<n;i++){UINT len=DragQueryFileW(hd,i,NULL,0)+1;pp[i]=malloc(len*sizeof(WCHAR));if(pp[i])DragQueryFileW(hd,i,pp[i],len);}AddPaths(pp,n);for(UINT i=0;i<n;i++)free(pp[i]);free(pp);}DragFinish(hd);return 0;}
    case WM_COMMAND:switch(LOWORD(wp)){
        case ID_FILE_SELALL: for(int i=0;i<g_fileCount;i++)ListView_SetCheckState(g_hListView,i,TRUE);SetStatus(L"已全选所有文件");break;
        case ID_FILE_INVERT: {for(int i=0;i<g_fileCount;i++)ListView_SetCheckState(g_hListView,i,!ListView_GetCheckState(g_hListView,i));WCHAR ms[64];_snwprintf(ms,64,L"已反选");SetStatus(ms);}break;
        case ID_MENU_REMOVE:{int*sel;int n=GetSelected(&sel);if(n>0){for(int i=n-1;i>=0;i--)RemoveFileAt(sel[i]);free(sel);}}break;
        case ID_CTX_LOADTO:{
            /* 找到右键点击的那行 (ListView_GetNextItem 反向) */
            int hitItem=-1;
            for(int i=0;i<g_fileCount;i++){if(ListView_GetCheckState(g_hListView,i)){hitItem=i;break;}}
            if(hitItem<0||hitItem>=g_fileCount){SetStatus(L"未选中任何文件");break;}

            /* 把所选文件的三个时间分别写回主配置区 */
            SetDateTimeControls(g_hCDatePicker,g_hCTimeEdit,&g_files[hitItem].ftCreate);
            SetDateTimeControls(g_hMDatePicker,g_hMTimeEdit,&g_files[hitItem].ftModify);
            SetDateTimeControls(g_hADatePicker,g_hATimeEdit,&g_files[hitItem].ftAccess);

            /* 主界面的修改项：全部勾上，便于一键同步三个属性 */
            SendMessageW(g_hChkCreate,BM_SETCHECK,BST_CHECKED,0);
            SendMessageW(g_hChkModify,BM_SETCHECK,BST_CHECKED,0);
            SendMessageW(g_hChkAccess,BM_SETCHECK,BST_CHECKED,0);

            WCHAR ms[160];
            _snwprintf(ms,160,L"已读取 [%s] 的三个时间到配置区（创建/修改/访问 均已勾选）",g_files[hitItem].path);
            SetStatus(ms);
        }break;
        case ID_BTN_ADDDIR:{BROWSEINFOW bi={0};bi.hwndOwner=hWnd;bi.lpszTitle=L"选择要添加的文件夹";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;LPITEMIDLIST pidl=SHBrowseForFolderW(&bi);if(pidl){WCHAR dir[MAX_PATH];SHGetPathFromIDListW(pidl,dir);WCHAR*p[1]={dir};AddPaths(p,1);CoTaskMemFree(pidl);}}break;
        case ID_BTN_CLEAR:g_fileCount=0;RefreshList();SetStatus(L"列表已清空");break;
        case ID_BTN_SETNOW:{FILETIME ft;GetSystemTimeAsFileTime(&ft);ApplyTime(&ft,&ft,&ft);break;}
        case ID_BTN_SETSPEC:{FILETIME ftC,ftM,ftA;if(!GetDateTimeControls(g_hCDatePicker,g_hCTimeEdit,&ftC)||!GetDateTimeControls(g_hMDatePicker,g_hMTimeEdit,&ftM)||!GetDateTimeControls(g_hADatePicker,g_hATimeEdit,&ftA)){MessageBoxW(g_hMainWnd,L"日期或时间格式错误，请使用 HH:MM:SS 格式。",L"提示",MB_ICONWARNING);break;}ApplyTime(&ftC,&ftM,&ftA);break;}
        case ID_BTN_SYNCMAIN:{
            SYSTEMTIME stM;WCHAR tbM[16]={0};
            DateTime_GetSystemtime(g_hMDatePicker,&stM);
            GetWindowTextW(g_hMTimeEdit,tbM,15);
            if(SendMessageW(g_hChkCreate,BM_GETCHECK,0,0)==BST_CHECKED){
                DateTime_SetSystemtime(g_hCDatePicker,GDT_VALID,&stM);
                SetWindowTextW(g_hCTimeEdit,tbM);
            }
            if(SendMessageW(g_hChkAccess,BM_GETCHECK,0,0)==BST_CHECKED){
                DateTime_SetSystemtime(g_hADatePicker,GDT_VALID,&stM);
                SetWindowTextW(g_hATimeEdit,tbM);
            }
            SetStatus(L"已同步修改时间到勾选的配置区");
            break;
        }
        case ID_BTN_FILEDATE:DoFilenameDate();break;
        case ID_BTN_MINUS1D:AdjustSelected(-864000000000LL);break;case ID_BTN_MINUS1H:AdjustSelected(-36000000000LL);break;
        case ID_BTN_PLUS1H:AdjustSelected(36000000000LL);break;case ID_BTN_PLUS1D:AdjustSelected(864000000000LL);break;
        case ID_BTN_CUSTOM:DoCustom();break;
        case ID_BTN_RANDTIME:DoRandomTime();break;
        case ID_BTN_SELALL:{for(int i=0;i<g_fileCount;i++)ListView_SetCheckState(g_hListView,i,TRUE);DoCustom();break;}
    }return 0;
    case WM_NOTIFY:{LPNMHDR nmh=(LPNMHDR)lp;
        if(nmh->idFrom==ID_LISTVIEW){
            if(nmh->code==NM_DBLCLK){
                LPNMITEMACTIVATE ia=(LPNMITEMACTIVATE)lp;
                WCHAR dbg[128];
                _snwprintf(dbg,128,L"DBG DBLCLK iItem=%d count=%d",ia->iItem,g_fileCount);
                SetStatus(dbg);
                if(ia->iItem>=0&&ia->iItem<g_fileCount)OpenEditDialog(ia->iItem);
            }
            else if(nmh->code==NM_RCLICK){LPNMITEMACTIVATE ia=(LPNMITEMACTIVATE)lp;if(ia->iItem>=0&&ia->iItem<g_fileCount){ListView_SetCheckState(g_hListView,ia->iItem,TRUE);HMENU hMenu=CreatePopupMenu();AppendMenuW(hMenu,MF_STRING,ID_CTX_LOADTO,L"读取选定文件属性到配置");AppendMenuW(hMenu,MF_SEPARATOR,0,NULL);AppendMenuW(hMenu,MF_STRING,ID_MENU_REMOVE,L"从列表移除");POINT pt;GetCursorPos(&pt);TrackPopupMenu(hMenu,TPM_LEFTALIGN|TPM_RIGHTBUTTON,pt.x,pt.y,0,hWnd,NULL);DestroyMenu(hMenu);}}
        }return 0;
    }
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{HDC hdc=(HDC)wp;SetBkColor(hdc,GetSysColor(COLOR_BTNFACE));return(LRESULT)g_hBgBrush;}
    case WM_GETMINMAXINFO:{MINMAXINFO*mm=(MINMAXINFO*)lp;mm->ptMinTrackSize.x=680;mm->ptMinTrackSize.y=510;mm->ptMaxTrackSize.y=mm->ptMinTrackSize.y;return 0;}
    case WM_CLOSE:DestroyWindow(hWnd);return 0;
    case WM_DESTROY:DragAcceptFiles(hWnd,FALSE);if(g_hFont)DeleteObject(g_hFont);if(g_hBgBrush)DeleteObject(g_hBgBrush);PostQuitMessage(0);return 0;
    }return DefWindowProcW(hWnd,msg,wp,lp);
}

int WINAPI wWinMain(HINSTANCE hInst,HINSTANCE hPrev,LPWSTR lpCmd,int nShow){
    (void)hPrev;g_hInst=hInst; srand(GetTickCount());
    INITCOMMONCONTROLSEX icc={sizeof(icc),ICC_WIN95_CLASSES|ICC_DATE_CLASSES};
    if(!InitCommonControlsEx(&icc)){MessageBoxW(NULL,L"初始化失败!",L"错误",MB_ICONERROR);return 1;}
    WNDCLASSEXW wc={sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WndProc;wc.hInstance=hInst;wc.hCursor=LoadCursorW(NULL,(LPCWSTR)IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.hIcon=LoadIconW(hInst,MAKEINTRESOURCEW(IDI_APP_ICON));wc.hIconSm=LoadIconW(hInst,MAKEINTRESOURCEW(IDI_APP_ICON));wc.lpszClassName=L"FtoolGUIWnd";
    if(!RegisterClassExW(&wc)){MessageBoxW(NULL,L"注册窗口类失败!",L"错误",MB_ICONERROR);return 1;}
    WNDCLASSEXW wce={sizeof(wce)};wce.style=CS_HREDRAW|CS_VREDRAW;wce.lpfnWndProc=EditDlgProc;wce.hInstance=hInst;wce.cbWndExtra=DLGWINDOWEXTRA;wce.hCursor=LoadCursorW(NULL,(LPCWSTR)IDC_ARROW);wce.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wce.lpszClassName=L"FtoolEditDlg";
    if(!RegisterClassExW(&wce)){
        DWORD e=GetLastError();
        if(e!=ERROR_CLASS_ALREADY_EXISTS){
            WCHAR er[80];_snwprintf(er,80,L"EditDlg 注册失败 err=%lu",e);
            MessageBoxW(NULL,er,L"错误",MB_ICONERROR);return 1;
        }
    }
    int tw=710,th=520,x=(GetSystemMetrics(SM_CXSCREEN)-tw)/2,y=(GetSystemMetrics(SM_CYSCREEN)-th)/2;
    HWND hWnd=CreateWindowExW(0,L"FtoolGUIWnd",L"文件日期修改工具 - ftool",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
        x,y,tw,th,NULL,NULL,hInst,NULL);
    if(!hWnd){MessageBoxW(NULL,L"创建窗口失败!",L"错误",MB_ICONERROR);return 1;}
    ShowWindow(hWnd,nShow);UpdateWindow(hWnd);
    if(lpCmd&&lpCmd[0]){int argc;WCHAR**argv=CommandLineToArgvW(lpCmd,&argc);if(argv&&argc>1){AddPaths(&argv[1],argc-1);LocalFree(argv);}}
    MSG msg;while(GetMessageW(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return(int)msg.wParam;
}
