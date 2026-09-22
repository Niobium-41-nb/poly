; *** Inno Setup version 6.5.0+ Chinese Simplified messages ***
;
; To download user-contributed translations of this file, go to:
;   https://jrsoftware.org/files/istrans/
;
; Note: When translating this text, do not add periods (.) to the end of
; messages that didn't have them already, because on those messages Inno
; Setup adds the periods automatically (appending a period would result in
; two periods being displayed).
;
; Maintainer: Zhenghan Yang (Kira)
; Email: 847320916@QQ.com
; Github: https://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation
; Encoding: UTF-8
; Translation based on network resource
;

[LangOptions]
; The following three entries are very important. Be sure to read and
; understand the '[LangOptions] section' topic in the help file.
LanguageName=绠€浣撲腑鏂?
; About LanguageID, to reference link:
; https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-lcid/a9eac961-e77d-41a6-90a5-ce1a8b0cdb9c
LanguageID=$0804
; LanguageCodePage should always be set if possible, even if this file is Unicode
; For English it's set to zero anyway because English only uses ASCII characters
LanguageCodePage=936
; If the language you are translating to requires special font faces or
; sizes, uncomment any of the following entries and change them accordingly.
;DialogFontName=
;DialogFontSize=9
;DialogFontBaseScaleWidth=7
;DialogFontBaseScaleHeight=15
;WelcomeFontName=Segoe UI
;WelcomeFontSize=14

[Messages]

; *** Application titles
SetupAppTitle=瀹夎
SetupWindowTitle=瀹夎 - %1
UninstallAppTitle=鍗歌浇
UninstallAppFullTitle=%1 鍗歌浇

; *** Misc. common
InformationTitle=淇℃伅
ConfirmTitle=纭
ErrorTitle=閿欒

; *** SetupLdr messages
SetupLdrStartupMessage=鐜板湪灏嗗畨瑁?%1銆傛偍鎯宠缁х画鍚楋紵
LdrCannotCreateTemp=鏃犳硶鍒涘缓涓存椂鏂囦欢銆傚畨瑁呯▼搴忓凡涓
LdrCannotExecTemp=鏃犳硶鎵ц涓存椂鐩綍涓殑鏂囦欢銆傚畨瑁呯▼搴忓凡涓
HelpTextNote=

; *** Startup error messages
LastErrorMessage=%1銆?n%n閿欒 %2: %3
SetupFileMissing=瀹夎鐩綍涓己灏戞枃浠?%1銆傝淇杩欎釜闂鎴栬€呰幏鍙栫▼搴忕殑鏂板壇鏈€?
SetupFileCorrupt=瀹夎鏂囦欢宸叉崯鍧忋€傝鑾峰彇绋嬪簭鐨勬柊鍓湰銆?
SetupFileCorruptOrWrongVer=瀹夎鏂囦欢宸叉崯鍧忥紝鎴栨槸涓庤繖涓畨瑁呯▼搴忕殑鐗堟湰涓嶅吋瀹广€傝淇杩欎釜闂鎴栬幏鍙栨柊鐨勭▼搴忓壇鏈€?
InvalidParameter=鏃犳晥鐨勫懡浠よ鍙傛暟锛?n%n%1
SetupAlreadyRunning=瀹夎绋嬪簭宸插湪杩愯銆?
WindowsVersionNotSupported=姝ょ▼搴忎笉鏀寔褰撳墠璁＄畻鏈鸿繍琛岀殑 Windows 鐗堟湰銆?
WindowsServicePackRequired=姝ょ▼搴忛渶瑕?%1 鏈嶅姟鍖?%2 鎴栨洿楂樼増鏈€?
NotOnThisPlatform=姝ょ▼搴忎笉鑳藉湪 %1 涓婅繍琛屻€?
OnlyOnThisPlatform=姝ょ▼搴忓彧鑳藉湪 %1 涓婅繍琛屻€?
OnlyOnTheseArchitectures=姝ょ▼搴忓彧鑳藉畨瑁呭埌涓轰笅鍒楀鐞嗗櫒鏋舵瀯璁捐鐨?Windows 鐗堟湰涓細%n%n%1
WinVersionTooLowError=姝ょ▼搴忛渶瑕?%1 鐗堟湰 %2 鎴栨洿楂樸€?
WinVersionTooHighError=姝ょ▼搴忎笉鑳藉畨瑁呬簬 %1 鐗堟湰 %2 鎴栨洿楂樸€?
AdminPrivilegesRequired=鍦ㄥ畨瑁呮绋嬪簭鏃舵偍蹇呴』浠ョ鐞嗗憳韬唤鐧诲綍銆?
PowerUserPrivilegesRequired=鍦ㄥ畨瑁呮绋嬪簭鏃舵偍蹇呴』浠ョ鐞嗗憳韬唤鎴栭珮绾х敤鎴风粍韬唤鐧诲綍銆?
SetupAppRunningError=瀹夎绋嬪簭妫€娴嬪埌 %1 褰撳墠姝ｅ湪杩愯銆?n%n璇峰厛鍏抽棴姝ｅ湪杩愯鐨勭▼搴忥紝鐒跺悗鐐瑰嚮鈥滅‘瀹氣€濈户缁紝鎴栫偣鍑烩€滃彇娑堚€濋€€鍑恒€?
UninstallAppRunningError=鍗歌浇绋嬪簭妫€娴嬪埌 %1 褰撳墠姝ｅ湪杩愯銆?n%n璇峰厛鍏抽棴姝ｅ湪杩愯鐨勭▼搴忥紝鐒跺悗鐐瑰嚮鈥滅‘瀹氣€濈户缁紝鎴栫偣鍑烩€滃彇娑堚€濋€€鍑恒€?

; *** Startup questions
PrivilegesRequiredOverrideTitle=閫夋嫨瀹夎绋嬪簭瀹夎妯″紡
PrivilegesRequiredOverrideInstruction=閫夋嫨瀹夎妯″紡
PrivilegesRequiredOverrideText1=%1 鍙互涓烘墍鏈夌敤鎴峰畨瑁咃紙闇€瑕佺鐞嗗憳鏉冮檺锛夛紝鎴栦粎涓烘偍瀹夎銆?
PrivilegesRequiredOverrideText2=%1 鍙互浠呬负鎮ㄥ畨瑁咃紝鎴栦负鎵€鏈夌敤鎴峰畨瑁咃紙闇€瑕佺鐞嗗憳鏉冮檺锛夈€?
PrivilegesRequiredOverrideAllUsers=涓烘墍鏈夌敤鎴峰畨瑁?&A)
PrivilegesRequiredOverrideAllUsersRecommended=涓烘墍鏈夌敤鎴峰畨瑁?&A)锛堟帹鑽愶級
PrivilegesRequiredOverrideCurrentUser=浠呬负鎴戝畨瑁?&M)
PrivilegesRequiredOverrideCurrentUserRecommended=浠呬负鎴戝畨瑁?&M)锛堟帹鑽愶級

; *** Misc. errors
ErrorCreatingDir=瀹夎绋嬪簭鏃犳硶鍒涘缓鐩綍鈥?1鈥?
ErrorTooManyFilesInDir=鏃犳硶鍦ㄧ洰褰曗€?1鈥濅腑鍒涘缓鏂囦欢锛屽洜涓洪噷闈㈠寘鍚お澶氭枃浠躲€?

; *** Setup common messages
ExitSetupTitle=閫€鍑哄畨瑁呯▼搴?
ExitSetupMessage=瀹夎绋嬪簭灏氭湭瀹屾垚銆傚鏋滅幇鍦ㄩ€€鍑猴紝灏嗕笉浼氬畨瑁呰绋嬪簭銆?n%n鎮ㄤ箣鍚庡彲浠ュ啀娆¤繍琛屽畨瑁呯▼搴忓畬鎴愬畨瑁呫€?n%n鐜板湪閫€鍑哄畨瑁呯▼搴忓悧锛?
AboutSetupMenuItem=鍏充簬瀹夎绋嬪簭(&A)...
AboutSetupTitle=鍏充簬瀹夎绋嬪簭
AboutSetupMessage=%1 鐗堟湰 %2%n%3%n%n%1 涓婚〉锛?n%4
AboutSetupNote=
TranslatorNote=绠€浣撲腑鏂囩炕璇戠敱 Kira锛?47320916@qq.com锛夌淮鎶ゃ€傞」鐩湴鍧€锛歨ttps://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation

; *** Buttons
ButtonBack=< 涓婁竴姝?&B)
ButtonNext=涓嬩竴姝?&N) >
ButtonInstall=瀹夎(&I)
ButtonOK=纭畾
ButtonCancel=鍙栨秷
ButtonYes=鏄?&Y)
ButtonYesToAll=鍏ㄦ槸(&A)
ButtonNo=鍚?&N)
ButtonNoToAll=鍏ㄥ惁(&O)
ButtonFinish=瀹屾垚(&F)
ButtonBrowse=娴忚(&B)...
ButtonWizardBrowse=娴忚(&R)...
ButtonNewFolder=鏂板缓鏂囦欢澶?&M)

; *** "Select Language" dialog messages
SelectLanguageTitle=閫夋嫨瀹夎璇█
SelectLanguageLabel=閫夋嫨瀹夎鏃朵娇鐢ㄧ殑璇█銆?

; *** Common wizard text
ClickNext=鐐瑰嚮鈥滀笅涓€姝モ€濈户缁紝鎴栫偣鍑烩€滃彇娑堚€濋€€鍑哄畨瑁呯▼搴忋€?
BeveledLabel=
BrowseDialogTitle=娴忚鏂囦欢澶?
BrowseDialogLabel=鍦ㄤ笅闈㈢殑鍒楄〃涓€夋嫨涓€涓枃浠跺す锛岀劧鍚庣偣鍑烩€滅‘瀹氣€濄€?
NewFolderName=鏂板缓鏂囦欢澶?

; *** "Welcome" wizard page
WelcomeLabel1=娆㈣繋浣跨敤 [name] 瀹夎鍚戝
WelcomeLabel2=鍗冲皢鍦ㄦ偍鐨勮绠楁満涓婂畨瑁?[name/ver]銆?n%n寤鸿鎮ㄥ湪缁х画瀹夎鍓嶅叧闂墍鏈夊叾浠栧簲鐢ㄧ▼搴忋€?

; *** "Password" wizard page
WizardPassword=瀵嗙爜
PasswordLabel1=姝ゅ畨瑁呯▼搴忛渶瑕佸瘑鐮侀獙璇併€?
PasswordLabel3=璇疯緭鍏ュ瘑鐮侊紝鐒跺悗鐐瑰嚮鈥滀笅涓€姝モ€濈户缁€傚瘑鐮佸尯鍒嗗ぇ灏忓啓銆?
PasswordEditLabel=瀵嗙爜(&P)锛?
IncorrectPassword=鎮ㄨ緭鍏ョ殑瀵嗙爜涓嶆纭紝璇烽噸鏂拌緭鍏ャ€?

; *** "License Agreement" wizard page
WizardLicense=璁稿彲鍗忚
LicenseLabel=璇峰湪缁х画瀹夎鍓嶉槄璇讳互涓嬮噸瑕佷俊鎭€?
LicenseLabel3=璇烽槄璇讳笅鍒楄鍙崗璁€傚湪缁х画瀹夎鍓嶆偍蹇呴』鍚屾剰杩欎簺鍗忚鏉℃銆?
LicenseAccepted=鎴戝悓鎰忔鍗忚(&A)
LicenseNotAccepted=鎴戜笉鍚屾剰姝ゅ崗璁?&D)

; *** "Information" wizard pages
WizardInfoBefore=淇℃伅
InfoBeforeLabel=璇峰湪缁х画瀹夎鍓嶉槄璇讳互涓嬮噸瑕佷俊鎭€?
InfoBeforeClickLabel=鍑嗗濂界户缁畨瑁呭悗锛岀偣鍑烩€滀笅涓€姝モ€濄€?
WizardInfoAfter=淇℃伅
InfoAfterLabel=璇峰湪缁х画瀹夎鍓嶉槄璇讳互涓嬮噸瑕佷俊鎭€?
InfoAfterClickLabel=鍑嗗濂界户缁畨瑁呭悗锛岀偣鍑烩€滀笅涓€姝モ€濄€?

; *** "User Information" wizard page
WizardUserInfo=鐢ㄦ埛淇℃伅
UserInfoDesc=璇疯緭鍏ユ偍鐨勪俊鎭€?
UserInfoName=鐢ㄦ埛鍚?&U)锛?
UserInfoOrg=缁勭粐(&O)锛?
UserInfoSerial=搴忓垪鍙?&S)锛?
UserInfoNameRequired=璇疯緭鍏ョ敤鎴峰悕銆?

; *** "Select Destination Location" wizard page
WizardSelectDir=閫夋嫨鐩爣浣嶇疆
SelectDirDesc=鎮ㄦ兂灏?[name] 瀹夎鍦ㄥ摢閲岋紵
SelectDirLabel3=瀹夎绋嬪簭灏嗗畨瑁?[name] 鍒颁笅闈㈢殑鏂囦欢澶逛腑銆?
SelectDirBrowseLabel=鐐瑰嚮鈥滀笅涓€姝モ€濈户缁€傚鏋滄偍鎯抽€夋嫨鍏朵粬鏂囦欢澶癸紝鐐瑰嚮鈥滄祻瑙堚€濄€?
DiskSpaceGBLabel=鑷冲皯闇€瑕佹湁 [gb] GB 鐨勫彲鐢ㄧ鐩樼┖闂淬€?
DiskSpaceMBLabel=鑷冲皯闇€瑕佹湁 [mb] MB 鐨勫彲鐢ㄧ鐩樼┖闂淬€?
CannotInstallToNetworkDrive=瀹夎绋嬪簭鏃犳硶瀹夎鍒颁竴涓綉缁滈┍鍔ㄥ櫒銆?
CannotInstallToUNCPath=瀹夎绋嬪簭鏃犳硶瀹夎鍒颁竴涓?UNC 璺緞銆?
InvalidPath=鎮ㄥ繀椤昏緭鍏ヤ竴涓甫椹卞姩鍣ㄧ洏绗︾殑瀹屾暣璺緞锛屼緥濡傦細%n%nC:\App%n%n鎴朥NC璺緞锛?n%n\\server\share
InvalidDrive=鎮ㄩ€夊畾鐨勯┍鍔ㄥ櫒鎴?UNC 鍏变韩涓嶅瓨鍦ㄦ垨涓嶈兘璁块棶銆傝閫夋嫨鍏朵粬浣嶇疆銆?
DiskSpaceWarningTitle=纾佺洏绌洪棿涓嶈冻
DiskSpaceWarning=瀹夎绋嬪簭鑷冲皯闇€瑕?%1 KB 鐨勫彲鐢ㄧ┖闂存墠鑳藉畨瑁咃紝浣嗛€夊畾椹卞姩鍣ㄥ彧鏈?%2 KB 鐨勫彲鐢ㄧ┖闂淬€?n%n鎮ㄧ‘瀹氳缁х画鍚楋紵
DirNameTooLong=鏂囦欢澶瑰悕绉版垨璺緞澶暱銆?
InvalidDirName=鏂囦欢澶瑰悕绉版棤鏁堛€?
BadDirName32=鏂囦欢澶瑰悕绉颁笉鑳藉寘鍚笅鍒椾换浣曞瓧绗︼細%n%n%1
DirExistsTitle=鏂囦欢澶瑰凡瀛樺湪
DirExists=鏂囦欢澶癸細%n%n%1%n%n宸茬粡瀛樺湪銆傛偍纭畾瀹夎鍒拌繖涓枃浠跺す涓悧锛?
DirDoesntExistTitle=鏂囦欢澶逛笉瀛樺湪
DirDoesntExist=鏂囦欢澶癸細%n%n%1%n%n涓嶅瓨鍦ㄣ€傛偍鎯宠鍒涘缓姝ゆ枃浠跺す鍚楋紵

; *** "Select Components" wizard page
WizardSelectComponents=閫夋嫨缁勪欢
SelectComponentsDesc=鎮ㄦ兂瀹夎鍝簺绋嬪簭缁勪欢锛?
SelectComponentsLabel2=閫変腑鎮ㄦ兂瀹夎鐨勭粍浠讹紱鍙栨秷鎮ㄤ笉鎯冲畨瑁呯殑缁勪欢銆傜劧鍚庣偣鍑烩€滀笅涓€姝モ€濈户缁€?
FullInstallation=瀹屽叏瀹夎
; if possible don't translate 'Compact' as 'Minimal' (I mean 'Minimal' in your language)
CompactInstallation=绠€娲佸畨瑁?
CustomInstallation=鑷畾涔夊畨瑁?
NoUninstallWarningTitle=缁勪欢宸插瓨鍦?
NoUninstallWarning=瀹夎绋嬪簭妫€娴嬪埌涓嬪垪缁勪欢宸插畨瑁呭湪鎮ㄧ殑璁＄畻鏈轰腑锛?n%n%1%n%n鍙栨秷閫変腑杩欎簺缁勪欢涓嶄細鍗歌浇瀹冧滑銆?n%n鎮ㄧ‘瀹氳缁х画鍚楋紵
ComponentSize1=%1 KB
ComponentSize2=%1 MB
ComponentsDiskSpaceGBLabel=褰撳墠閫夋嫨鐨勭粍浠堕渶瑕佽嚦灏?[gb] GB 鐨勭鐩樼┖闂淬€?
ComponentsDiskSpaceMBLabel=褰撳墠閫夋嫨鐨勭粍浠堕渶瑕佽嚦灏?[mb] MB 鐨勭鐩樼┖闂淬€?

; *** "Select Additional Tasks" wizard page
WizardSelectTasks=閫夋嫨闄勫姞浠诲姟
SelectTasksDesc=鎮ㄦ兂瑕佸畨瑁呯▼搴忔墽琛屽摢浜涢檮鍔犱换鍔★紵
SelectTasksLabel2=閫夋嫨鎮ㄦ兂瑕佸畨瑁呯▼搴忓湪瀹夎 [name] 鏃舵墽琛岀殑闄勫姞浠诲姟锛岀劧鍚庣偣鍑烩€滀笅涓€姝モ€濄€?

; *** "Select Start Menu Folder" wizard page
WizardSelectProgramGroup=閫夋嫨寮€濮嬭彍鍗曟枃浠跺す
SelectStartMenuFolderDesc=瀹夎绋嬪簭搴旇鍦ㄥ摢閲屾斁缃▼搴忕殑蹇嵎鏂瑰紡锛?
SelectStartMenuFolderLabel3=瀹夎绋嬪簭灏嗗湪涓嬪垪鈥滃紑濮嬧€濊彍鍗曟枃浠跺す涓垱寤虹▼搴忕殑蹇嵎鏂瑰紡銆?
SelectStartMenuFolderBrowseLabel=鐐瑰嚮鈥滀笅涓€姝モ€濈户缁€傚鏋滄偍鎯抽€夋嫨鍏朵粬鏂囦欢澶癸紝鐐瑰嚮鈥滄祻瑙堚€濄€?
MustEnterGroupName=鎮ㄥ繀椤昏緭鍏ヤ竴涓枃浠跺す鍚嶇О銆?
GroupNameTooLong=鏂囦欢澶瑰悕绉版垨璺緞澶暱銆?
InvalidGroupName=鏂囦欢澶瑰悕绉版棤鏁堛€?
BadGroupName=鏂囦欢澶瑰悕绉颁笉鑳藉寘鍚笅鍒椾换浣曞瓧绗︼細%n%n%1
NoProgramGroupCheck2=涓嶅垱寤哄紑濮嬭彍鍗曟枃浠跺す(&D)

; *** "Ready to Install" wizard page
WizardReady=鍑嗗瀹夎
ReadyLabel1=瀹夎绋嬪簭鍑嗗灏辩华锛岀幇鍦ㄥ彲浠ュ紑濮嬪畨瑁?[name] 鍒版偍鐨勮绠楁満銆?
ReadyLabel2a=鐐瑰嚮鈥滃畨瑁呪€濈户缁瀹夎绋嬪簭銆傚鏋滄偍鎯抽噸鏂版煡鐪嬫垨淇敼浠讳綍璁剧疆锛岀偣鍑烩€滀笂涓€姝モ€濄€?
ReadyLabel2b=鐐瑰嚮鈥滃畨瑁呪€濈户缁瀹夎绋嬪簭銆?
ReadyMemoUserInfo=鐢ㄦ埛淇℃伅锛?
ReadyMemoDir=鐩爣浣嶇疆锛?
ReadyMemoType=瀹夎绫诲瀷锛?
ReadyMemoComponents=宸查€夋嫨缁勪欢锛?
ReadyMemoGroup=寮€濮嬭彍鍗曟枃浠跺す锛?
ReadyMemoTasks=闄勫姞浠诲姟锛?

; *** TDownloadWizardPage wizard page and DownloadTemporaryFile
DownloadingLabel2=姝ｅ湪涓嬭浇鏂囦欢...
ButtonStopDownload=鍋滄涓嬭浇(&S)
StopDownload=鎮ㄧ‘瀹氳鍋滄涓嬭浇鍚楋紵
ErrorDownloadAborted=涓嬭浇宸蹭腑姝€?
ErrorDownloadFailed=涓嬭浇澶辫触锛?1 %2銆?
ErrorDownloadSizeFailed=鑾峰彇澶у皬澶辫触锛?1 %2銆?
ErrorProgress=鏃犳晥鐨勮繘搴︼細%1 / %2銆?
ErrorFileSize=鏂囦欢澶у皬閿欒锛氶鏈?%1锛屽疄闄?%2銆?

; *** TExtractionWizardPage wizard page and ExtractArchive
ExtractingLabel=姝ｅ湪鎻愬彇鏂囦欢...
ButtonStopExtraction=鍋滄鎻愬彇(&S)
StopExtraction=鎮ㄧ‘瀹氳鍋滄鎻愬彇鍚楋紵
ErrorExtractionAborted=鎻愬彇宸蹭腑姝€?
ErrorExtractionFailed=鎻愬彇澶辫触锛?1

; *** Archive extraction failure details
ArchiveIncorrectPassword=瀵嗙爜涓嶆纭€?
ArchiveIsCorrupted=鍘嬬缉鍖呭凡鎹熷潖銆?
ArchiveUnsupportedFormat=涓嶆敮鎸佺殑鍘嬬缉鍖呮牸寮忋€?

; *** "Preparing to Install" wizard page
WizardPreparing=姝ｅ湪鍑嗗瀹夎
PreparingDesc=瀹夎绋嬪簭姝ｅ湪鍑嗗瀹夎 [name] 鍒版偍鐨勮绠楁満銆?
PreviousInstallNotCompleted=鍏堝墠鐨勭▼搴忓畨瑁呮垨鍗歌浇鏈畬鎴愶紝闇€瑕佹偍閲嶅惎璁＄畻鏈轰互瀹屾垚璇ュ畨瑁呫€?n%n鍦ㄩ噸鍚绠楁満鍚庯紝鍐嶆杩愯瀹夎绋嬪簭浠ュ畬鎴?[name] 鐨勫畨瑁呫€?
CannotContinue=瀹夎绋嬪簭涓嶈兘缁х画銆傝鐐瑰嚮鈥滃彇娑堚€濋€€鍑恒€?
ApplicationsFound=浠ヤ笅搴旂敤绋嬪簭姝ｅ湪浣跨敤灏嗙敱瀹夎绋嬪簭鏇存柊鐨勬枃浠躲€傚缓璁偍鍏佽瀹夎绋嬪簭鑷姩鍏抽棴杩欎簺搴旂敤绋嬪簭銆?
ApplicationsFound2=浠ヤ笅搴旂敤绋嬪簭姝ｅ湪浣跨敤灏嗙敱瀹夎绋嬪簭鏇存柊鐨勬枃浠躲€傚缓璁偍鍏佽瀹夎绋嬪簭鑷姩鍏抽棴杩欎簺搴旂敤绋嬪簭銆傚畨瑁呭畬鎴愬悗锛屽畨瑁呯▼搴忓皢灏濊瘯閲嶆柊鍚姩杩欎簺搴旂敤绋嬪簭銆?
CloseApplications=鑷姩鍏抽棴搴旂敤绋嬪簭(&A)
DontCloseApplications=涓嶈鍏抽棴搴旂敤绋嬪簭(&D)
ErrorCloseApplications=瀹夎绋嬪簭鏃犳硶鑷姩鍏抽棴鎵€鏈夊簲鐢ㄧ▼搴忋€傚缓璁偍鍦ㄧ户缁箣鍓嶏紝鍏抽棴鎵€鏈夊湪浣跨敤闇€瑕佺敱瀹夎绋嬪簭鏇存柊鐨勬枃浠剁殑搴旂敤绋嬪簭銆?
PrepareToInstallNeedsRestart=瀹夎绋嬪簭蹇呴』閲嶅惎鎮ㄧ殑璁＄畻鏈恒€傝绠楁満閲嶅惎鍚庯紝璇峰啀娆¤繍琛屽畨瑁呯▼搴忎互瀹屾垚 [name] 鐨勫畨瑁呫€?n%n瑕佺珛鍗抽噸鍚悧锛?

; *** "Installing" wizard page
WizardInstalling=姝ｅ湪瀹夎
InstallingLabel=瀹夎绋嬪簭姝ｅ湪瀹夎 [name] 鍒版偍鐨勮绠楁満锛岃绋嶅€欍€?

; *** "Setup Completed" wizard page
FinishedHeadingLabel=瀹屾垚 [name] 瀹夎鍚戝
FinishedLabelNoIcons=瀹夎绋嬪簭宸插湪鎮ㄧ殑璁＄畻鏈轰腑瀹夎浜?[name]銆?
FinishedLabel=瀹夎绋嬪簭宸插湪鎮ㄧ殑璁＄畻鏈轰腑瀹夎浜?[name]銆傛偍鍙互閫氳繃宸插畨瑁呯殑蹇嵎鏂瑰紡杩愯姝ゅ簲鐢ㄧ▼搴忋€?
ClickFinish=鐐瑰嚮鈥滃畬鎴愨€濋€€鍑哄畨瑁呯▼搴忋€?
FinishedRestartLabel=涓哄畬鎴?[name] 鐨勫畨瑁咃紝瀹夎绋嬪簭蹇呴』閲嶆柊鍚姩鎮ㄧ殑璁＄畻鏈恒€傝绔嬪嵆閲嶅惎鍚楋紵
FinishedRestartMessage=涓哄畬鎴?[name] 鐨勫畨瑁咃紝瀹夎绋嬪簭蹇呴』閲嶆柊鍚姩鎮ㄧ殑璁＄畻鏈恒€?n%n瑕佺珛鍗抽噸鍚悧锛?
ShowReadmeCheck=鏄紝鎴戞兂鏌ラ槄鑷堪鏂囦欢
YesRadio=鏄紝绔嬪嵆閲嶅惎璁＄畻鏈?&Y)
NoRadio=鍚︼紝绋嶅悗閲嶅惎璁＄畻鏈?&N)
; used for example as 'Run MyProg.exe'
RunEntryExec=杩愯 %1
; used for example as 'View Readme.txt'
RunEntryShellExec=鏌ラ槄 %1

; *** "Setup Needs the Next Disk" stuff
ChangeDiskTitle=瀹夎绋嬪簭闇€瑕佷笅涓€寮犵鐩?
SelectDiskLabel2=璇锋彃鍏ョ鐩?%1 骞剁偣鍑烩€滅‘瀹氣€濄€?n%n濡傛灉杩欎釜纾佺洏涓殑鏂囦欢鍙互鍦ㄤ笅鍒楁枃浠跺す涔嬪鐨勬枃浠跺す涓壘鍒帮紝璇疯緭鍏ユ纭殑璺緞鎴栫偣鍑烩€滄祻瑙堚€濄€?
PathLabel=璺緞(&P)锛?
FileNotInDir2=鈥?2鈥濅腑鎵句笉鍒版枃浠垛€?1鈥濄€傝鎻掑叆姝ｇ‘鐨勭鐩樻垨閫夋嫨鍏朵粬鏂囦欢澶广€?
SelectDirectoryLabel=璇锋寚瀹氫笅涓€寮犵鐩樼殑浣嶇疆銆?

; *** Installation phase messages
SetupAborted=瀹夎绋嬪簭鏈畬鎴愬畨瑁呫€?n%n璇蜂慨姝ｈ繖涓棶棰樺苟閲嶆柊杩愯瀹夎绋嬪簭銆?
AbortRetryIgnoreSelectAction=閫夋嫨鎿嶄綔
AbortRetryIgnoreRetry=閲嶈瘯(&T)
AbortRetryIgnoreIgnore=蹇界暐閿欒骞剁户缁?&I)
AbortRetryIgnoreCancel=鍙栨秷瀹夎
RetryCancelSelectAction=閫夋嫨鎿嶄綔
RetryCancelRetry=閲嶈瘯(&T)
RetryCancelCancel=鍙栨秷

; *** Installation status messages
StatusClosingApplications=姝ｅ湪鍏抽棴搴旂敤绋嬪簭...
StatusCreateDirs=姝ｅ湪鍒涘缓鐩綍...
StatusExtractFiles=姝ｅ湪鎻愬彇鏂囦欢...
StatusDownloadFiles=姝ｅ湪涓嬭浇鏂囦欢...
StatusCreateIcons=姝ｅ湪鍒涘缓蹇嵎鏂瑰紡...
StatusCreateIniEntries=姝ｅ湪鍒涘缓 INI 鏉＄洰...
StatusCreateRegistryEntries=姝ｅ湪鍒涘缓娉ㄥ唽琛ㄦ潯鐩?..
StatusRegisterFiles=姝ｅ湪娉ㄥ唽鏂囦欢...
StatusSavingUninstall=姝ｅ湪淇濆瓨鍗歌浇淇℃伅...
StatusRunProgram=姝ｅ湪瀹屾垚瀹夎...
StatusRestartingApplications=姝ｅ湪閲嶅惎搴旂敤绋嬪簭...
StatusRollback=姝ｅ湪鎾ら攢鏇存敼...

; *** Misc. errors
ErrorInternal2=鍐呴儴閿欒锛?1銆?
ErrorFunctionFailedNoCode=%1 澶辫触銆?
ErrorFunctionFailed=%1 澶辫触锛涢敊璇唬鐮?%2銆?
ErrorFunctionFailedWithMessage=%1 澶辫触锛涢敊璇唬鐮?%2銆?n%3
ErrorExecutingProgram=鏃犳硶鎵ц鏂囦欢锛?n%1

; *** Registry errors
ErrorRegOpenKey=鎵撳紑娉ㄥ唽琛ㄩ」鏃跺嚭閿欙細%n%1\%2
ErrorRegCreateKey=鍒涘缓娉ㄥ唽琛ㄩ」鏃跺嚭閿欙細%n%1\%2
ErrorRegWriteKey=鍐欏叆娉ㄥ唽琛ㄩ」鏃跺嚭閿欙細%n%1\%2

; *** INI errors
ErrorIniEntry=鍦ㄦ枃浠垛€?1鈥濅腑鍒涘缓 INI 鏉＄洰鏃跺嚭閿欍€?

; *** File copying errors
FileAbortRetryIgnoreSkipNotRecommended=璺宠繃姝ゆ枃浠?&S)锛堜笉鎺ㄨ崘锛?
FileAbortRetryIgnoreIgnoreNotRecommended=蹇界暐閿欒骞剁户缁?&I)锛堜笉鎺ㄨ崘锛?
SourceIsCorrupted=婧愭枃浠跺凡鎹熷潖銆?
SourceDoesntExist=婧愭枃浠垛€?1鈥濅笉瀛樺湪銆?
SourceVerificationFailed=婧愭枃浠堕獙璇佸け璐ワ細%1
VerificationSignatureDoesntExist=绛惧悕鏂囦欢鈥?1鈥濅笉瀛樺湪銆?
VerificationSignatureInvalid=绛惧悕鏂囦欢鈥?1鈥濇棤鏁堛€?
VerificationKeyNotFound=绛惧悕鏂囦欢鈥?1鈥濅娇鐢ㄤ簡鏈煡鐨勫瘑閽ャ€?
VerificationFileNameIncorrect=鏂囦欢鍚嶄笉姝ｇ‘銆?
VerificationFileTagIncorrect=鏂囦欢鏍囩涓嶆纭€?
VerificationFileSizeIncorrect=鏂囦欢澶у皬涓嶆纭€?
VerificationFileHashIncorrect=鏂囦欢鍝堝笇鍊间笉姝ｇ‘銆?
ExistingFileReadOnly2=鏃犳硶鏇挎崲宸插瓨鍦ㄧ殑鏂囦欢锛屽畠鏄彧璇荤殑銆?
ExistingFileReadOnlyRetry=绉婚櫎鍙灞炴€у苟閲嶈瘯(&R)
ExistingFileReadOnlyKeepExisting=淇濈暀宸插瓨鍦ㄧ殑鏂囦欢(&K)
ErrorReadingExistingDest=灏濊瘯璇诲彇宸插瓨鍦ㄧ殑鏂囦欢鏃跺嚭閿欙細
FileExistsSelectAction=閫夋嫨鎿嶄綔
FileExists2=鏂囦欢宸茬粡瀛樺湪銆?
FileExistsOverwriteExisting=瑕嗙洊宸插瓨鍦ㄧ殑鏂囦欢(&O)
FileExistsKeepExisting=淇濈暀宸插瓨鍦ㄧ殑鏂囦欢(&K)
FileExistsOverwriteOrKeepAll=涓烘帴涓嬫潵鐨勫啿绐佹枃浠舵墽琛屾鎿嶄綔(&D)
ExistingFileNewerSelectAction=閫夋嫨鎿嶄綔
ExistingFileNewer2=宸插瓨鍦ㄧ殑鏂囦欢姣斿畨瑁呯▼搴忓皢瑕佸畨瑁呯殑鏂囦欢杩樿鏂般€?
ExistingFileNewerOverwriteExisting=瑕嗙洊宸插瓨鍦ㄧ殑鏂囦欢(&O)
ExistingFileNewerKeepExisting=淇濈暀宸插瓨鍦ㄧ殑鏂囦欢(&K)锛堟帹鑽愶級
ExistingFileNewerOverwriteOrKeepAll=涓烘帴涓嬫潵鐨勫啿绐佹枃浠舵墽琛屾鎿嶄綔(&D)
ErrorChangingAttr=灏濊瘯鏇存敼涓嬪垪宸插瓨鍦ㄧ殑鏂囦欢灞炴€ф椂鍑洪敊锛?
ErrorCreatingTemp=灏濊瘯鍦ㄧ洰鏍囩洰褰曞垱寤烘枃浠舵椂鍑洪敊锛?
ErrorReadingSource=灏濊瘯璇诲彇涓嬪垪婧愭枃浠舵椂鍑洪敊锛?
ErrorCopying=灏濊瘯澶嶅埗涓嬪垪鏂囦欢鏃跺嚭閿欙細
ErrorDownloading=灏濊瘯涓嬭浇鏂囦欢鏃跺嚭閿欙細
ErrorExtracting=灏濊瘯鎻愬彇鍘嬬缉鍖呮椂鍑洪敊锛?
ErrorReplacingExistingFile=灏濊瘯鏇挎崲宸插瓨鍦ㄧ殑鏂囦欢鏃跺嚭閿欙細
ErrorRestartReplace=閲嶅惎骞舵浛鎹㈠け璐ワ細
ErrorRenamingTemp=灏濊瘯閲嶅懡鍚嶄笅鍒楃洰鏍囩洰褰曚腑鐨勪竴涓枃浠舵椂鍑洪敊锛?
ErrorRegisterServer=鏃犳硶娉ㄥ唽 DLL/OCX锛?1
ErrorRegSvr32Failed=RegSvr32 澶辫触锛涢€€鍑轰唬鐮?%1銆?
ErrorRegisterTypeLib=鏃犳硶娉ㄥ唽绫诲瀷搴擄細%1

; *** Uninstall display name markings
; used for example as 'My Program (32-bit)'
UninstallDisplayNameMark=%1 (%2)
; used for example as 'My Program (32-bit, All users)'
UninstallDisplayNameMarks=%1 (%2, %3)
UninstallDisplayNameMark32Bit=32 浣?
UninstallDisplayNameMark64Bit=64 浣?
UninstallDisplayNameMarkAllUsers=鎵€鏈夌敤鎴?
UninstallDisplayNameMarkCurrentUser=褰撳墠鐢ㄦ埛

; *** Post-installation errors
ErrorOpeningReadme=灏濊瘯鎵撳紑鑷堪鏂囦欢鏃跺嚭閿欍€?
ErrorRestartingComputer=瀹夎绋嬪簭鏃犳硶閲嶅惎璁＄畻鏈猴紝璇锋墜鍔ㄩ噸鍚€?

; *** Uninstaller messages
UninstallNotFound=鏂囦欢鈥?1鈥濅笉瀛樺湪銆傛棤娉曞嵏杞姐€?
UninstallOpenError=鏂囦欢鈥?1鈥濅笉鑳借鎵撳紑銆傛棤娉曞嵏杞?
UninstallUnsupportedVer=姝ょ増鏈殑鍗歌浇绋嬪簭鏃犳硶璇嗗埆鍗歌浇鏃ュ織鏂囦欢鈥?1鈥濈殑鏍煎紡銆傛棤娉曞嵏杞姐€?
UninstallUnknownEntry=鍗歌浇鏃ュ織涓亣鍒颁竴涓湭鐭ユ潯鐩紙%1锛夈€?
ConfirmUninstall=鎮ㄧ‘璁よ瀹屽叏绉婚櫎 %1 鍙婂叾鎵€鏈夌粍浠跺悧锛?
UninstallOnlyOnWin64=浠呭厑璁稿湪 64 浣?Windows 涓嵏杞芥绋嬪簭銆?
OnlyAdminCanUninstall=浠呬娇鐢ㄧ鐞嗗憳鏉冮檺鐨勭敤鎴疯兘瀹屾垚姝ゅ嵏杞姐€?
UninstallStatusLabel=姝ｅ湪浠庢偍鐨勮绠楁満涓Щ闄?%1锛岃绋嶅€欍€?
UninstalledAll=宸查『鍒╀粠鎮ㄧ殑璁＄畻鏈轰腑绉婚櫎 %1銆?
UninstalledMost=%1 鍗歌浇瀹屾垚銆?n%n鏈夐儴鍒嗗唴瀹规湭鑳借鍒犻櫎锛屼絾鎮ㄥ彲浠ユ墜鍔ㄥ垹闄ゅ畠浠€?
UninstalledAndNeedsRestart=涓哄畬鎴?%1 鐨勫嵏杞斤紝闇€瑕侀噸鍚偍鐨勮绠楁満銆?n%n瑕佺珛鍗抽噸鍚悧锛?
UninstallDataCorrupted=鏂囦欢鈥?1鈥濆凡鎹熷潖銆傛棤娉曞嵏杞姐€?

; *** Uninstallation phase messages
ConfirmDeleteSharedFileTitle=鍒犻櫎鍏变韩鏂囦欢锛?
ConfirmDeleteSharedFile2=绯荤粺琛ㄧず涓嬪垪鍏变韩鏂囦欢宸蹭笉鍐嶆湁浠讳綍绋嬪簭浣跨敤銆傛偍甯屾湜鍗歌浇绋嬪簭鍒犻櫎姝ゅ叡浜枃浠跺悧锛?n%n濡傛灉浠嶆湁绋嬪簭姝ｅ湪浣跨敤姝ゆ枃浠讹紝鍒犻櫎鍚庤繖浜涚▼搴忓彲鑳芥棤娉曟甯歌繍琛屻€傚鏋滄偍涓嶈兘纭畾锛岃閫夋嫨鈥滃惁鈥濓紝淇濈暀姝ゆ枃浠跺湪绯荤粺涓笉浼氶€犳垚浠讳綍鎹熷銆?
SharedFileNameLabel=鏂囦欢鍚嶏細
SharedFileLocationLabel=浣嶇疆锛?
WizardUninstalling=鍗歌浇鐘舵€?
StatusUninstalling=姝ｅ湪鍗歌浇 %1...

; *** Shutdown block reasons
ShutdownBlockReasonInstallingApp=姝ｅ湪瀹夎 %1銆?
ShutdownBlockReasonUninstallingApp=姝ｅ湪鍗歌浇 %1銆?

; The custom messages below aren't used by Setup itself, but if you make
; use of them in your scripts, you'll want to translate them.

[CustomMessages]

NameAndVersion=%1 鐗堟湰 %2
AdditionalIcons=闄勫姞蹇嵎鏂瑰紡锛?
CreateDesktopIcon=鍒涘缓妗岄潰蹇嵎鏂瑰紡(&D)
CreateQuickLaunchIcon=鍒涘缓蹇€熷惎鍔ㄦ爮蹇嵎鏂瑰紡(&Q)
ProgramOnTheWeb=%1 缃戠珯
UninstallProgram=鍗歌浇 %1
LaunchProgram=杩愯 %1
AssocFileExtension=灏?%2 鏂囦欢鎵╁睍鍚嶄笌 %1 寤虹珛鍏宠仈(&A)
AssocingFileExtension=姝ｅ湪灏?%2 鏂囦欢鎵╁睍鍚嶄笌 %1 寤虹珛鍏宠仈...
AutoStartProgramGroupDescription=鍚姩锛?
AutoStartProgram=鑷姩鍚姩 %1
AddonHostProgramNotFound=鎮ㄩ€夋嫨鐨勬枃浠跺す涓棤娉曟壘鍒?%1銆?n%n鎮ㄧ‘瀹氳缁х画鍚楋紵
