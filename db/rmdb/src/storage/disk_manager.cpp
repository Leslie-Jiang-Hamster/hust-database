#include "storage/disk_manager.h"

#include <assert.h>    // for assert
#include <string.h>    // for memset
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for lseek
#include <iostream>

#include "defs.h"

static void Lseek(int fd, off_t offset, int whence) {
    if (lseek(fd, offset, whence) < 0) {
        perror("lseek");
        throw UnixError{};
    }
}

static void Write(int fd, const void *buf, size_t count) {
    if (write(fd, buf, count) < 0) {
        perror("write");
        throw UnixError{};
    }
}

static void Read(int fd, void *buf, size_t count) {
    if (read(fd, buf, count) < 0) {
        perror("read");
        throw UnixError{};
    }
}

static int Open(const char *path, int flags) {
    int fd = -1;
    if ((fd = open(path, flags)) < 0) {
        perror("open");
        throw UnixError{};
    }
    return fd;
}

static void Close(int fd) {
    if (close(fd) < 0) {
        perror("close");
        throw UnixError{};
    }
}

static void Unlink(const char *pathname) {
    if (unlink(pathname) < 0) {
        perror("unlink");
        throw UnixError{};
    }
}

DiskManager::DiskManager() { 
    memset(fd2pageno_, 0, MAX_FD * (sizeof(std::atomic<page_id_t>) / sizeof(char))); 
}

/**
 * @brief Write the contents of the specified page into disk file
 *
 * @description: 将数据写入文件的指定磁盘页面中
 * @param {int} fd 磁盘文件的文件句柄
 * @param {page_id_t} page_no 写入目标页面的page_id
 * @param {char} *offset 要写入磁盘的数据
 * @param {int} num_bytes 要写入磁盘的数据大小
 */

void DiskManager::write_page(int fd, page_id_t page_no, const char *offset, int num_bytes) {
    // Todo:
    // 1.lseek()定位到文件头，通过(fd,page_no)可以定位指定页面及其在磁盘文件中的偏移量
    // 2.调用write()函数
    // 注意处理异常
    // 'offset' is a misleading name, which actually refers to the data about to be written

    assert(offset != nullptr);
    assert(num_bytes <= PAGE_SIZE);
    Lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    Write(fd, offset, num_bytes);
}

/**
 * @brief Read the contents of the specified page into the given memory area
 *
 * @description: 读取文件中指定编号的页面中的部分数据到内存中
 * @param {int} fd 磁盘文件的文件句柄
 * @param {page_id_t} page_no 指定的页面编号
 * @param {char} *offset 读取的内容写入到offset中
 * @param {int} num_bytes 读取的数据量大小
 */
void DiskManager::read_page(int fd, page_id_t page_no, char *offset, int num_bytes) {
    // Todo:
    // 1.lseek()定位到文件头，通过(fd,page_no)可以定位指定页面及其在磁盘文件中的偏移量
    // 2.调用read()函数
    // 注意处理异常
    // 'offset' is a misleading name, which actually refers to the data about to be written
    assert(offset != nullptr);
    assert(num_bytes <= PAGE_SIZE);
    Lseek(fd, page_no * PAGE_SIZE, SEEK_SET);
    Read(fd, offset, num_bytes);
}

/**
 * @brief Allocate new page (operations like create index/table)
 * For now just keep an increasing counter
 * 
 * @description: 分配一个新的页号
 * @return {page_id_t} 分配的新页号
 * @param {int} fd 指定文件的文件句柄
 */
page_id_t DiskManager::AllocatePage(int fd) {
    // Todo:
    // 简单的自增分配策略，指定文件的页面编号加1
    assert(fd >= 0 && fd < MAX_FD);
    return fd2pageno_[fd]++; 
}
  

/**
 * @brief Deallocate page (operations like drop index/table)
 * Need bitmap in header page for tracking pages
 * This does not actually need to do anything for now.
 */
void DiskManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {

}

bool DiskManager::is_dir(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void DiskManager::create_dir(const std::string &path) {
    // Create a subdirectory
    std::string cmd = "mkdir " + path;
    if (system(cmd.c_str()) < 0) {  // 创建一个名为path的目录
        throw UnixError();
    }
}

void DiskManager::destroy_dir(const std::string &path) {
    std::string cmd = "rm -r " + path;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

/**
 * @brief 用于判断指定路径文件是否存在 
 */
bool DiskManager::is_file(const std::string &path) {
    // Todo:
    // 用struct stat获取文件信息
    struct stat st;
    if (stat(path.c_str(), &st) < 0) {
        return false;
    } 
    return S_ISREG(st.st_mode);
}

/**
 * @brief 用于创建指定路径文件
 */
void DiskManager::create_file(const std::string &path) {
    // Todo:
    // 调用open()函数，使用O_CREAT模式
    // 注意不能重复创建相同文件
    struct stat st;
    if (stat(path.c_str(), &st) >= 0) {
        throw FileExistsError{path};
    }
    int fd = Open(path.c_str(), O_CREAT);
    Close(fd);
}

/**
 * @brief 用于删除指定路径文件 
 */
void DiskManager::destroy_file(const std::string &path) {
    // Todo:
    // 调用unlink()函数
    // 注意不能删除未关闭的文件
    struct stat st;
    if (path2fd_.find(path) != path2fd_.end()) {
        throw FileNotFoundError{path};
    }
    if (stat(path.c_str(), &st) < 0) {
        throw FileNotFoundError{path};
    }
    Unlink(path.c_str());
}

/**
 * @brief 用于打开指定路径文件
 * 返回值的用途??????????????  权当return file descritor
 */
int DiskManager::open_file(const std::string &path) {
    // Todo:
    // 调用open()函数，使用O_RDWR模式
    // 注意不能重复打开相同文件，并且需要更新文件打开列表
    int fd = -1;
    if ( is_file(path) == 0 ) {
        throw FileNotFoundError(path);
    } 
    
    auto search = path2fd_.find(path);

    if ( search == path2fd_.end() ) {
        fd = open( path.c_str(), O_RDWR);
        //path2fd_.insert(std::make_pair(path, fd));
        //fd2path_.insert(std::make_pair(fd, path));
        path2fd_[path] = fd;
        fd2path_[fd] = path;
    }
    else {
        fd = path2fd_[path];
    }

    return fd;  
}

/**
 * @brief 用于关闭指定路径文件
 */
void DiskManager::close_file(int fd) {
    // Todo:
    // 调用close()函数
    // 注意不能关闭未打开的文件，并且需要更新文件打开列表

    auto it = fd2path_.find(fd);
    if( it != fd2path_.end() ) {
        close(fd);
        const std::string path = fd2path_[fd];
        path2fd_.erase(path);
        //path2fd_.erase( path2fd_.find((*it).second) );
        fd2path_.erase(fd);
    } else {
        throw FileNotOpenError(fd);
    }
}

int DiskManager::GetFileSize(const std::string &file_name) {
    struct stat stat_buf;
    int rc = stat(file_name.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

std::string DiskManager::GetFileName(int fd) {
    if (!fd2path_.count(fd)) {
        throw FileNotOpenError(fd);
    }
    return fd2path_[fd];
}

int DiskManager::GetFileFd(const std::string &file_name) {
    if (!path2fd_.count(file_name)) {
        return open_file(file_name);
    }
    return path2fd_[file_name];
}

bool DiskManager::ReadLog(char *log_data, int size, int offset, int prev_log_end) {
    // read log file from the previous end
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }
    offset += prev_log_end;
    int file_size = GetFileSize(LOG_FILE_NAME);
    if (offset >= file_size) {
        return false;
    }

    size = std::min(size, file_size - offset);
    lseek(log_fd_, offset, SEEK_SET);
    ssize_t bytes_read = read(log_fd_, log_data, size);
    if (bytes_read != size) {
        throw UnixError();
    }
    return true;
}

void DiskManager::WriteLog(char *log_data, int size) {
    if (log_fd_ == -1) {
        log_fd_ = open_file(LOG_FILE_NAME);
    }

    // write from the file_end
    lseek(log_fd_, 0, SEEK_END);
    ssize_t bytes_write = write(log_fd_, log_data, size);
    if (bytes_write != size) {
        throw UnixError();
    }
}
