#include <tr1/memory>
#include "ftp/listener.hpp"
#include "util/net/tlscontext.hpp"
#include "util/net/error.hpp"
#include "logger/logger.hpp"
#include "fs/owner.hpp"
#include "fs/path.hpp"
#include "cfg/config.hpp"
#include "cfg/get.hpp"
#include "cfg/exception.hpp"

const char* dummySiteRootPtr = getenv("FTPD_ROOT");
extern const fs::Path dummySiteRoot(dummySiteRootPtr ?  dummySiteRootPtr :
                                    "/home/bioboy/ftpd/site");

#ifndef TEST

int main(int argc, char** argv)
{
  try
  {
    cfg::Config *config = new cfg::Config("ftpd.conf");
    cfg::UpdateShared(std::tr1::shared_ptr<cfg::Config>(config));
  }
  catch (const cfg::ConfigError& e)
  {
    logger::error << e.what() << logger::endl;
    return 1;
  }
  
  try
  {
    const std::string& certificate = cfg::Get()->TlsCertificate()->ToString();
    util::net::TLSServerContext::Initialise(certificate, "");
  }
  catch (const util::net::NetworkError& e)
  {
    logger::error << "TLS failed to initialise: " << e.Message() << logger::endl;
    return 1;
  }

  fs::OwnerCache::Start();
  
  ftp::Listener listener("::", 1234);  
  if (!listener.Initialise())
  {
    logger::error << "Listener failed to initialise!" << logger::endl;
    return 1;
  }
  
  listener.Start();
  listener.Join();
  
  fs::OwnerCache::Stop();
  
  (void) argc;
  (void) argv;
}

#endif
