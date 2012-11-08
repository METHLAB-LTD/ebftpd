#include <cctype>
#include <boost/algorithm/string.hpp>
#include "text/tag.hpp"
#include "text/error.hpp"
#include "logs/logs.hpp"

namespace text
{

void Tag::Register(const std::string& filter)
{
  logs::debug << "Tag::Register: " << filter << logs::endl;
  if (filter == "left")
    alignment = Alignment::Left;

  else if (filter == "right")
    alignment = Alignment::Right;

  else if (filter == "center")
    alignment = Alignment::Center;

  else if (filter == "kb")
  {
    SetType(TagType::Decimal);
    measurement = Measurement::Kbyte;
  }
  else if (filter == "mb")
  {
    SetType(TagType::Decimal);
    measurement = Measurement::Mbyte;
  }
  else if (filter == "gb")
  {
    SetType(TagType::Decimal);
    measurement = Measurement::Gbyte;
  }
  else if (filter == "auto")
    measurement = Measurement::Auto;

  else
  {
    std::vector<std::string> args;
    boost::split(args, filter, boost::is_any_of(".")); 
    if (args.size() > 2) throw TemplateFilterMalform("Error parsing filter, should be '^\\d+(\\.\\d+)?$': " + filter);
    for (auto& c: args.front())
      if (!std::isdigit(c)) throw TemplateFilterMalform("Error parsing filter, should be an integer: " + args.front());
    width = args.front();
  
    if (args.size() == 2)
    {
      for (auto& c: args.back())
        if (!std::isdigit(c)) 
          throw TemplateFilterMalform("Error parsing filter, should be an integer: " + args.back());
      precision = args.back();
      SetType(TagType::Float);
    }
  }

}

void Tag::Compile()
{ 
  std::ostringstream os;
  os << "%";

  if (alignment == Alignment::Left)
    os << "-";
  else if (alignment == Alignment::Center)
    os << "=";

  if (!width.empty())
  {
    os << width;  
    if (!precision.empty()) os << "." << precision;
  }

  if (type == TagType::String) os << "s";
  else if (type == TagType::Float) os << "f";
  else if (type == TagType::Decimal) os << "d";
  else assert(false); // should never get here


  format = os.str();
}

// end
}
