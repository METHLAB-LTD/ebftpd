#include <cassert>
#include <algorithm>
#include <iterator>
#include "util/passwd.hpp"
#include "acl/user.hpp"
#include "acl/types.hpp"
#include "db/user/user.hpp"

namespace acl
{

User::User(const std::string& name,
           const std::string& password,
           const std::string& flags) :
  modified(boost::posix_time::microsec_clock::local_time()),
  name(name),
  uid(-1),
  primaryGid(-1),
  credits(0)
{
  SetPassword(password);
  AddFlags(flags);
}

void User::SetPassword(const std::string& password)
{
  using namespace util::passwd;
  
  modified = boost::posix_time::microsec_clock::local_time();
  std::string rawSalt = GenerateSalt();
  this->password = HexEncode(HashPassword(password, rawSalt));
  salt = HexEncode(rawSalt);
}

bool User::VerifyPassword(const std::string& password) const
{
  using namespace util::passwd;
  
  return HexEncode(HashPassword(password, HexDecode(salt))) == this->password;
}

void User::SetFlags(const std::string& flags)
{
  modified = boost::posix_time::microsec_clock::local_time();
  assert(ValidFlags(flags));
  this->flags = flags;
}

void User::AddFlags(const std::string& flags)
{
  modified = boost::posix_time::microsec_clock::local_time();
  assert(ValidFlags(flags));
  for (char ch: flags)
  {
    if (this->flags.find(ch) == std::string::npos) this->flags += ch;
  }
  
  std::sort(this->flags.begin(), this->flags.end());
}

void User::AddFlag(Flag flag)
{
  AddFlags(std::string(1, static_cast<char>(flag)));
}

void User::DelFlags(const std::string& flags)
{
  modified = boost::posix_time::microsec_clock::local_time();
  for (char ch: flags)
  {
    std::string::size_type pos = this->flags.find(ch);
    if (pos != std::string::npos) this->flags.erase(pos, 1);
  }  
}

void User::DelFlag(Flag flag)
{
  DelFlags(std::string(1, static_cast<char>(flag)));
}

bool User::CheckFlags(const std::string& flags) const
{
  for (char ch: flags)
  {
    if (this->flags.find(ch) != std::string::npos) return true;
  }
  return false;
}

bool User::CheckFlag(Flag flag) const
{
  return this->flags.find(static_cast<char>(flag)) != std::string::npos;
}

void User::SetPrimaryGID(GroupID primaryGid)
{
  modified = boost::posix_time::microsec_clock::local_time();
  assert(primaryGid >= -1);
  this->primaryGid = primaryGid;
}

void User::AddSecondaryGID(GroupID gid)
{
  modified = boost::posix_time::microsec_clock::local_time();
  secondaryGids.push_back(gid);
}

void User::DelSecondaryGID(GroupID gid)
{
  modified = boost::posix_time::microsec_clock::local_time();
  secondaryGids.erase(std::remove(secondaryGids.begin(), secondaryGids.end(), gid), 
      secondaryGids.end());
}

void User::ResetSecondaryGIDs()
{
  modified = boost::posix_time::microsec_clock::local_time();
  secondaryGids.clear();
}

bool User::HasSecondaryGID(GroupID gid)
{
  return std::find(secondaryGids.begin(), secondaryGids.end(), gid) != secondaryGids.end();
}

bool User::CheckGID(GroupID gid)
{
  return primaryGid == gid || HasSecondaryGID(gid);
}

} /* acl namespace */

#ifdef ACL_USER_TEST

#include <iostream>

int main()
{
  using namespace acl;
  
  User u("bioboy", acl::UserID(1), "password", "1");
  
  std::cout << u.VerifyPassword("test1234") << std::endl;
  std::cout << u.VerifyPassword("password") << std::endl;
  u.SetPassword("wowsers");
  std::cout << "pass changed" << std::endl;
  std::cout << u.VerifyPassword("wowsers") << std::endl;
  std::cout << u.VerifyPassword("w0000000000t") << std::endl;
  
  
  std::cout << u.Flags() << std::endl;
  u.AddFlags("789");
  std::cout << u.Flags() << std::endl;
  u.DelFlags("17");
  std::cout << u.Flags() << std::endl;

}

#endif
