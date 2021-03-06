#include "User.h"
#include "Utility.h"
#include <iostream>
#include <fstream>
using namespace std;

extern FileManager g_FileManager;

User::User() {
    u_error = U_NOERROR;			// 清错误标志位
    fileManager = &g_FileManager;	// 文件管理器指针初始化
	u_dirp = "/";					// 当前目录父目录：根目录
    u_curdir = "/";					// 当前目录：根目录
    
	u_cdir = fileManager->rootDirInode;			// rootNode
    u_pdir = NULL;								// NULL
    Utility::memset(u_arg, 0, sizeof(u_arg));	// 清参数
}

User::~User() {
}

/*************************************
 * 创建目录
 * 
 *************************************/
void User::Mkdir(string dirName) {
	// checkPathName检查路径是否合法
	// 并且将路径放置在u_dirp中
    if (!checkPathName(dirName)) {
        return;
    }
    u_arg[1] = Inode::IFDIR;	// u_arg[1]存放imode
    fileManager->Creat();		// 创建目录
    IsError();					// 检查是否有错
}

void User::Ls() {
    ls.clear();					// 清string
    fileManager->Ls();			// ls遍历当前目录结点所有非空目录项并输出
    if (IsError()) {
        return;
    }
    cout << ls;
}

void User::Cd(string dirName) {
    if (!checkPathName(dirName)) {
        return;
    }
    fileManager->ChDir();
    IsError();
}

void User::Create(string fileName, string mode) {
    if (!checkPathName(fileName)) {
        return;
    }
    int md = INodeMode(mode);
    if (md == 0) {
        cout << "this mode is undefined !  \n";
        return;
    }

    u_arg[1] = md;
    fileManager->Creat();
    IsError();
}

void User::Delete(string fileName) {
    if (!checkPathName(fileName)) {
        return;
    }
    fileManager->UnLink();
    IsError();
}

void User::Open(string fileName, string mode) {
    if (!checkPathName(fileName)) {
        return;
    }
    int md = FileMode(mode);
    if (md == 0) {
        cout << "this mode is undefined ! \n";
        return;
    }

    u_arg[1] = md;
    fileManager->Open();
    if (IsError())
        return;
    cout << "open success, return fd=" << u_ar0[EAX] << endl;
}

void User::Close(string sfd) {
    if (sfd.empty() || !isdigit(sfd.front())) {
        cout << "parameter fd can't be empty or be nonnumeric ! \n";
        return;
    }
    u_arg[0] = stoi(sfd);;
    fileManager->Close();
    IsError();
}

void User::Seek(string sfd, string offset, string origin) {
    if (sfd.empty() || !isdigit(sfd.front())) {
        cout << "parameter fd can't be empty or be nonnumeric ! \n";
        return;
    }
    if (offset.empty()) {
        cout << "parameter offset can't be empty ! \n";
        return;
    }
    if (origin.empty() || !isdigit(origin.front())) {
        cout << "parameter origin can't be empty or be nonnumeric ! \n";
        return;
    }
    u_arg[0] = stoi(sfd);
    u_arg[1] = stoi(offset);
    u_arg[2] = stoi(origin);
    fileManager->Seek();
    IsError();
}

void User::write(string fileName, string inFile, string size, string mode) {
    Open(fileName,mode);

    int fd = u_ar0[EAX];
    write(fd, inFile, size);

    Close(to_string(fd));
}

void User::write(int fd, string inFile, string size) {
    int usize = stoi(size);
    char* buffer;
    if (size.empty() || usize < 0) {
        // 写全部文件
        ifstream fin(inFile, ios::in | ios::binary | ios::ate );
        if (!fin) {
            cout << "file " << inFile << " open failed ! \n";
            return;
        }
        // TODO: here's a warning.tellg returns a token and may not convert to an int
        ifstream::pos_type pos = fin.tellg();
        int length = pos;
        cout<<length<<endl;
        if (length<0){
            cout << "file length " << length << " invalid ! \n";
            return;
        }

        usize = length;
        buffer = new char[length];
        fin.seekg(0, ios::beg);
        fin.read(buffer, length);
        fin.close();
    } else {
        buffer = new char[usize];
        fstream fin(inFile, ios::in | ios::binary);
        if (!fin) {
            cout << "file " << inFile << " open failed ! \n";
            return;
        }
        fin.read(buffer, usize);
        fin.close();
    }

    u_arg[0] = fd;
    u_arg[1] = (long)buffer;
    u_arg[2] = usize;
    fileManager->Write();

    if (IsError())
        return;
    cout << "write " << u_ar0[User::EAX] << "bytes success !" << endl;
    delete[]buffer;
}

void User::Write(string sfd, string inFile, string size) {
    if (sfd.empty()) {
        cout << "parameter fd can't be empty! \n";
        return;
    }

    if (size.empty()){
        cout << "parameter size can't be empty! \n";
        return;
    }

    if (!isdigit(sfd.front())){
        // 以读文件的方式读
        // 读完关闭文件
        string filename = sfd;
        write(filename, inFile, size, "-rw");
        return;
    }
    int fd = stoi(sfd);
    write(fd,inFile,size);
}

void User::readImage(string fileName, int fd) {
    
    int usize = u_ofiles.GetF(fd)->f_inode->i_size;
    char* buffer = new char[usize];

    u_arg[0] = fd;
    u_arg[1] = (long)buffer;
    u_arg[2] = usize;
    fileManager->Read();
    if (IsError())
        return;

    cout << "read " << u_ar0[User::EAX] << " bytes success : \n" ;

    fstream fout(fileName, ios::out | ios::binary);
    if (!fout) {
        cout << "file " << fileName << " open failed ! \n";
        return;
    }
    fout.write(buffer, u_ar0[User::EAX]);
    fout.close();

    system(string("open "+fileName).c_str());

    Close(to_string(fd));
}

//TODO: 加上特定后缀文件指定api
void User::read(string fileName, string outFile, string size, string mode) {
    Open(fileName,mode);
    int fd = u_ar0[EAX];

    // 识别图片
    if (Utility::has_suffix(fileName,".jpg") || Utility::has_suffix(fileName,".JPG") || Utility::has_suffix(fileName,".PNG") || Utility::has_suffix(fileName,".png")){
        readImage(fileName,fd);
        return;
    }

    read(fd,outFile,size);

    Close(to_string(fd));
}

void User::read(int fd, string outFile, string size) {
    int usize = stoi(size);
    if (usize < 0){
        usize = u_ofiles.GetF(fd)->f_inode->i_size;
    }

    char* buffer = new char[usize];

    u_arg[0] = fd;
    u_arg[1] = (long)buffer;
    u_arg[2] = usize;
    fileManager->Read();
    if (IsError())
        return;

    cout << "read " << u_ar0[User::EAX] << " bytes success : \n" ;
    if (outFile.empty()) {
        for (unsigned int i = 0; i < u_ar0[User::EAX]; ++i) {
            cout << (char)buffer[i];
        }
        cout << " \n";
        return;
    }
    fstream fout(outFile, ios::out | ios::binary);
    if (!fout) {
        cout << "file " << outFile << " open failed ! \n";
        return;
    }
    fout.write(buffer, u_ar0[User::EAX]);
    fout.close();
    cout << "read to " << outFile << " done ! \n";
    delete[]buffer;
}

void User::Read(string sfd, string outFile, string size) {
    if (sfd.empty()) {
        cout << "parameter fd can't be empty! \n";
        return;
    }

    if (size.empty()){
        cout << "parameter size can't be empty! \n";
        return;
    }

    if (!isdigit(sfd.front())){
        // 以读文件的方式读
        // 读完关闭文件
        string filename = sfd;
        read(filename, outFile, size, "-r");
        return;
    }

    int fd = stoi(sfd);

    //以读fd的方式读文件
    read(fd, outFile, size);
}

void User::Copy(string srcFile, string desPath){
    if (!checkPathName(srcFile)) {
        return;
    }

    Inode* srcInode = fileManager->NameI(FileManager::OPEN);
    string fileName = string(u_dbuf);
    if (srcInode==NULL){
        cout << "src file not found!"<<endl;
        return;
    }

    if(!checkPathName(desPath)) {
        return;
    }
    u_dirp+="/*";
    fileManager->NameI(FileManager::CREATE);
    if (u_pdir==NULL || u_pdir->i_mode & Inode::IFMT != Inode::IFDIR){
        cout << "destination invalid!"<<endl;
        return;
    }

    memcpy(u_dbuf,fileName.c_str(),sizeof(char)*(fileName.length()+1));
    fileManager->WriteDir(srcInode);
    IsError();					// 检查是否有错
}

void User::Move(string srcFile, string desPath){
    if (!checkPathName(srcFile)) {
        return;
    }

    Inode* srcInode = fileManager->NameI(FileManager::OPEN);
    string fileName = string(u_dbuf);
    if (srcInode==NULL){
        cout << "src file not found!"<<endl;
        return;
    }

    if(!checkPathName(desPath)) {
        return;
    }
    if (u_dirp[u_dirp.length()-1]!='/'){
        u_dirp+="/***";
    }
    else{
        u_dirp+="***";
    }

    fileManager->NameI(FileManager::CREATE);
    if (u_pdir==NULL || u_pdir->i_mode & Inode::IFMT != Inode::IFDIR){
        cout << "destination invalid!"<<endl;
        return;
    }

    memcpy(u_dbuf,fileName.c_str(),sizeof(char)*(fileName.length()+1));
    fileManager->WriteDir(srcInode);
    IsError();					// 检查是否有错
    Delete(srcFile);
}

void User::Cat(string srcFile){
    Open(srcFile,"-r");
    int fd = u_ar0[EAX];
    File* file = u_ofiles.GetF(fd);
    if ( NULL == file )
	{
        IsError();
		return;
	}

    char buf[200];

    u_IOParam.m_Offset = 0;
    u_IOParam.m_Count = min(file->f_inode->i_size,200);
    u_IOParam.m_Base = (unsigned char*)buf;
    file->f_inode->ReadI();
    cout<<buf<<endl;

    while(u_IOParam.m_Offset < file->f_inode->i_size){
        string command;
        cin>>command;
        cout<<endl;
        if (command=="n"){
            u_IOParam.m_Count = min(file->f_inode->i_size - u_IOParam.m_Offset,200);
            u_IOParam.m_Base = (unsigned char*)buf;
            file->f_inode->ReadI();
            cout<<buf<<endl;
            continue;
        }else if(command=="q"){
            break;
        }else{
            continue;
        }
    }
    IsError();
    Close(to_string(fd));
    IsError();
}

int User::INodeMode(string mode) {
    int md = 0;
    if (mode.find("-r") != string::npos) {
        md |= Inode::IREAD;
    }
    if (mode.find("-w") != string::npos) {
        md |= Inode::IWRITE;
    }
    if (mode.find("-rw") != string::npos) {
        md |= (Inode::IREAD | Inode::IWRITE);
    }
    return md;
}

int User::FileMode(string mode) {
    int md = 0;
    if (mode.find("-r") != string::npos) {
        md |= File::FREAD;
    }
    if (mode.find("-w") != string::npos) {
        md |= File::FWRITE;
    }
    if (mode.find("-rw") != string::npos) {
        md |= (File::FREAD | File::FWRITE);
    }
    return md;
}

/*************************************
 * 检查输入path的合理性
 * 
 * 将path中的相对路径转为绝对路径并放入u_dirp中
 * 
 * 只接受开头为..的路径，中间出现..或者.将无法处理
 *************************************/
bool User::checkPathName(string path) {
    // FileManager 中函数不判断参数的合法性，最好在User中过滤，
    // 暂不考虑过多的参数不合法情况
    if (path.empty()) {
        cout << "parameter path can't be empty ! \n";
        return false;
    }

    if (path.substr(0, 2) != "..") {
        u_dirp = path;
    }
    else {
        string pre = u_curdir;
        unsigned int p = 0;
        //可以多重相对路径 .. ../ ../.. ../../
        for (; pre.length() > 3 && p < path.length() && path.substr(p, 2) == ".."; ) {
            pre.pop_back();
            pre.erase(pre.find_last_of('/') + 1);
            p += 2;
            p += p < path.length() && path[p] == '/';
        }
        u_dirp = pre + path.substr(p);
    }

    if (u_dirp.length() > 1 && u_dirp.back() == '/') {
        u_dirp.pop_back();
    }

    for (unsigned int p = 0, q = 0; p < u_dirp.length(); p = q + 1) {
        q = u_dirp.find('/', p);
        q = min(q, (unsigned int)u_dirp.length());
        if (q - p > DirectoryEntry::DIRSIZ) {
            cout << "the fileName or dirPath can't be greater than 28 size ! \n";
            return false;
        }
    }
    return true;
}

bool User::IsError() {
    if (u_error != U_NOERROR) {
        cout << "errno = " << u_error;
        EchoError(u_error);
        u_error = U_NOERROR;
        return true;
    }
    return false;
}

void User::EchoError(enum ErrorCode err) {
    string estr;
    switch (err) {
    case User::U_NOERROR:
        estr = " No u_error ";
        break;
    case User::U_ENOENT:
        estr = " No such file or directory ";
        break;
    case User::U_EBADF:
        estr = " Bad file number ";
        break;
    case User::U_EACCES:
        estr = " Permission denied ";
        break;
    case User::U_ENOTDIR:
        estr = " Not a directory ";
        break;
    case User::U_ENFILE:
        estr = " File table overflow ";
        break;
    case User::U_EMFILE:
        estr = " Too many open files ";
        break;
    case User::U_EFBIG:
        estr = " File too large ";
        break;
    case User::U_ENOSPC:
        estr = " No space left on device ";
        break;
    default:
        break;
    }
    cout << estr << endl;
}

// show current user info status
void User::debug(){
    cout << "dirpath is: " << u_dirp <<endl;
    cout << "u_curdir is: " << u_curdir << endl;
    cout << "u_dbuf is: " << u_dbuf << endl;
    cout << " -------------IO Param------------------"<<endl;
    cout << "m_base is:" << u_IOParam.m_Base << endl;
    cout << "m_count is:" << u_IOParam.m_Count << endl;
    cout << "m_offset is:" << u_IOParam.m_Offset << endl;
    u_cdir->debug();
    u_pdir->debug();
}