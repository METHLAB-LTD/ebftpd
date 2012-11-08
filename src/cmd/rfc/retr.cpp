#include <ios>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cmd/rfc/retr.hpp"
#include "fs/file.hpp"
#include "acl/usercache.hpp"
#include "db/stats/stat.hpp"
#include "stats/util.hpp"
#include "util/scopeguard.hpp"
#include "ftp/counter.hpp"
#include "ftp/util.hpp"
#include "logs/logs.hpp"

namespace cmd { namespace rfc
{

cmd::Result RETRCommand::Execute()
{
  namespace pt = boost::posix_time;
  using util::scope_guard;
  using util::make_guard;
  
  off_t offset = data.RestartOffset();
  if (offset > 0 && data.DataType() == ftp::DataType::ASCII)
  {
    control.Reply(ftp::BadCommandSequence, "Resume not supported on ASCII data type.");
    return cmd::Result::Okay;
  }
  
  if (!ftp::Counter::StartDownload(client.User().UID(), client.Profile().MaxSimDl()))
  {
    std::ostringstream os;
    os << "You have reached your maximum of " << client.Profile().MaxSimDl() 
       << " simultaenous download(s).";
    control.Reply(ftp::ActionNotOkay, os.str());
    return cmd::Result::Okay;    
  }
  
  scope_guard countGuard = make_guard([&]{ ftp::Counter::StopDownload(client.User().UID()); });  

  fs::FileSourcePtr fin;
  try
  {
    fin = fs::OpenFile(client, argStr);
  }
  catch (const util::SystemError& e)
  {
    control.Reply(ftp::ActionNotOkay,
                 "Unable to open file: " + e.Message());
    return cmd::Result::Okay;
  }

  std::streampos size;
  try
  {
    size = fin->seek(0, std::ios_base::end);  
    if (offset > size)
    {
      control.Reply(ftp::InvalidRESTParameter, "Restart offset larger than file size.");
      return cmd::Result::Okay;
    }

    fin->seek(offset, std::ios_base::beg);
  }
  catch (const std::ios_base::failure& e)
  {
    std::string errmsg = e.what();
    errmsg[0] = std::toupper(errmsg[0]);
    control.Reply(ftp::ActionAbortedError, errmsg);
  }

  std::stringstream os;
  os << "Opening " << (data.DataType() == ftp::DataType::ASCII ? "ASCII" : "BINARY") 
     << " connection for download of " 
     << fs::Path(argStr).Basename().ToString()
     << " (" << size << " bytes)";
  if (data.Protection()) os << " using TLS/SSL";
  os << ".";
  control.Reply(ftp::TransferStatusOkay, os.str());

  try
  {
    data.Open(ftp::TransferType::Download);
  }
  catch (const util::net::NetworkError&e )
  {
    control.Reply(ftp::CantOpenDataConnection,
                 "Unable to open data connection: " +
                 e.Message());
    return cmd::Result::Okay;
  }

  std::streamsize bytes = 0;
  try
  {
    std::vector<char> asciiBuf;
    char buffer[16384];
    while (true)
    {
      std::streamsize len = fin->read(buffer, sizeof(buffer));
      if (len < 0) break;
      
      data.State().Update(len);
      
      char *bufp = buffer;
      if (data.DataType() == ftp::DataType::ASCII)
      {
        ftp::util::ASCIITranscodeRETR(buffer, len, asciiBuf);
        len = asciiBuf.size();
        bufp = asciiBuf.data();
      }
      
      data.Write(bufp, len);

      if (client.Profile().MaxDlSpeed() > 0)
        ftp::util::SpeedLimitSleep(data.State(), client.Profile().MaxDlSpeed());
    }
  }
  catch (const std::ios_base::failure& e)
  {
    fin->close();
    data.Close();
    control.Reply(ftp::DataCloseAborted,
                 "Error while reading from disk: " + std::string(e.what()));
    return cmd::Result::Okay;
  }
  catch (const util::net::NetworkError& e)
  {
    fin->close();
    data.Close();
    control.Reply(ftp::DataCloseAborted,
                 "Error while writing to data connection: " +
                 e.Message());
    return cmd::Result::Okay;
  }
  
  fin->close();
  data.Close();
  pt::time_duration duration = data.State().EndTime() - data.State().StartTime();
  db::stats::Download(client.User(), bytes / 1024, duration.total_milliseconds());
  acl::UserCache::DecrCredits(client.User().Name(), bytes / 1024);

  control.Reply(ftp::DataClosedOkay, "Transfer finished @ " + 
      stats::util::AutoUnitSpeedString(stats::util::CalculateSpeed(data.State().Bytes(), duration))); 
  return cmd::Result::Okay;
  
  (void) countGuard;
}

} /* rfc namespace */
} /* cmd namespace */
