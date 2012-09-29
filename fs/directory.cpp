#include <sys/stat.h>
#include <cerrno>
#include <cassert>
#include "fs/directory.hpp"
#include "fs/status.hpp"
#include "acl/user.hpp"
#include "ftp/client.hpp"
#include "fs/owner.hpp"
#include "fs/direnumerator.hpp"

#include <iostream>
#include "logger/logger.hpp"

extern const fs::Path dummySiteRoot;

namespace fs
{
namespace 
{

util::Error RemoveDirectory(const Path& path)
{
  Path real = dummySiteRoot + path;
  
  DirEnumerator dirEnum;
  
  try
  {
    dirEnum.Readdir(real);
  }
  catch (const util::SystemError& e)
  {
    return util::Error::Failure(e.Errno());
  }
  
  for (DirEnumerator::const_iterator it =
       dirEnum.begin(); it != dirEnum.end(); ++it)
  {
    if (it->Path().ToString()[0] != '.' ||
        it->Status().IsDirectory() ||
        !it->Status().IsWriteable())
      return util::Error::Failure(ENOTEMPTY);
  }
  
  for (DirEnumerator::const_iterator it =
       dirEnum.begin(); it != dirEnum.end(); ++it)
  {
    fs::Path fullPath = real / it->Path();
    if (unlink(fullPath.CString()) < 0)
      return util::Error::Failure(errno);
    OwnerCache::Delete(fullPath);
  }
    
  logger::ftpd << "real: " << real << logger::endl;
  if (rmdir(real.CString()) < 0) return util::Error::Failure(errno);
  OwnerCache::Delete(real);
  return util::Error::Success();
}

// we don't literally change the the working dir of the process
// as the ftpd is multithreaded and this would change work dir
// of all threads, we instead just check that chdir is possible
util::Error ChangeDirectory(const Path& path)
{
  Path real = dummySiteRoot + path;
  
  try
  {
    Status stat(real);
    if (!stat.IsDirectory()) return util::Error::Failure(ENOTDIR);
    if (!stat.IsExecutable()) return util::Error::Failure(EACCES);
  }
  catch (const util::SystemError& e)
  {
    return util::Error::Failure(e.Errno());
  }
  catch (const util::RuntimeError& e)
  {
    return util::Error::Failure();
  }
  
  return util::Error::Success();
}

}

util::Error CreateDirectory(ftp::Client& client, const Path& path)
{
  Path absolute = (client.WorkDir() / path).Expand();
  
  // check ACLs here
  
  Path real = dummySiteRoot + absolute;
  if (mkdir(real.CString(), 0777) < 0) return util::Error::Failure(errno);
  
  OwnerCache::Chown(real, Owner(client.User().UID(), client.User().PrimaryGID()));
  
  return util::Error::Success();
}

util::Error RemoveDirectory(ftp::Client& client, const Path& path)
{
  Path absolute = (client.WorkDir() / path).Expand();
  
  // check ACLs here
  logger::ftpd << "absolute: " << absolute << logger::endl;
  return RemoveDirectory(absolute);
}

util::Error ChangeDirectory(ftp::Client& client, const Path& path)
{
  Path absolute = (client.WorkDir() / path).Expand();
  // check ACLs here
  
  util::Error e = ChangeDirectory(absolute);
  if (e) client.SetWorkDir(absolute);
  return e;
}

} /* fs namespace */

#ifdef FS_DIRECTORY_TEST

#include <iostream>

int main()
{
  using namespace fs;
  
  util::Error e = CreateDirectory("/tmp");
  if (!e) std::cout << "mkdir failed: " << e.Message() << std::endl;
  else std::cout << "mkdir okay" << std::endl;
  
  e = RemoveDirectory("/tmp/something");
  if (!e) std::cout << "rmdir failed: " << e.Message() << std::endl;
  else std::cout << "rmdir okay" << std::endl;
  
  e = ChangeDirectory("/tmp");
  if (!e) std::cout << "chdir failed: " << e.Message() << std::endl;
  else std::cout << "chdir okay" << std::endl;

  e = ChangeDirectory("/tmp/notexist");
  if (!e) std::cout << "chdir failed: " << e.Message() << std::endl;
  else std::cout << "chdir okay" << std::endl;

  ftp::Client client;
  std::cout << "mkdir: " << CreateDirectory(client, "/w000t").Message() << std::endl;
  std::cout << "chdir: " << ChangeDirectory(client, "/w000t").Message() << std::endl;
  std::cout << "chdir: " << ChangeDirectory(client, "/something").Message() << std::endl;
  std::cout << "rmdir: " << RemoveDirectory(client, "/w000t").Message() << std::endl;
  std::cout << "mkdir: " << CreateDirectory(client, "wow").Message() << std::endl;
  std::cout << "rmdir: " << RemoveDirectory(client, "wow").Message() << std::endl;
}

#endif
