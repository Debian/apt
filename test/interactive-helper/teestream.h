#ifndef APT_HELPER_TEESTREAM_H
#define APT_HELPER_TEESTREAM_H

/* 'basic' implementation of a streambuf which passes the output
   to two 'real' streambufs emulating '| tee' on the shell

   The main use is streaming debug output to std::clog as well as
   a logfile easily, so don't expect that to be a bulletproof
   implementation. */

#include <iostream>
#include <apt-pkg/macros.h>

template <typename CharT, typename Traits = std::char_traits<CharT>
> class basic_teebuf: public std::basic_streambuf<CharT, Traits>
{
public:
   basic_teebuf(std::basic_streambuf<CharT, Traits> * const sb1,
	 std::basic_streambuf<CharT, Traits> * const sb2)
      : s1(sb1), s2(sb2) {}
protected:
   virtual std::streamsize xsputn(const CharT* s, std::streamsize c) APT_OVERRIDE
   {
      return s2->sputn(s, s1->sputn(s, c));
   }
   // overflow is the fallback of sputc which is non-virtual
   typedef typename Traits::int_type int_type;
   virtual int_type overflow(int_type ch = Traits::eof()) APT_OVERRIDE
   {
      auto const eof = Traits::eof();
      if (Traits::eq_int_type(ch, Traits::eof()) == true)
	 return eof;

      auto const r1 = s1->sputc(Traits::to_char_type(ch));
      auto const r2 = s2->sputc(Traits::to_char_type(ch));
      return Traits::eq_int_type(r1, eof) ? r1: r2;
   }
   virtual void imbue(const std::locale& loc) APT_OVERRIDE
   {
      s1->pubimbue(loc);
      s2->pubimbue(loc);
   }
   virtual int sync() APT_OVERRIDE
   {
      auto const r1 = s1->pubsync();
      auto const r2 = s2->pubsync();
      return r1 == 0 ? r2 : r1;
   }
private:
   std::basic_streambuf<CharT, Traits> * const s1;
   std::basic_streambuf<CharT, Traits> * const s2;
};
template <typename CharT, typename Traits = std::char_traits<CharT>
> class basic_teeostream: public std::basic_ostream<CharT, Traits>
{
public:
    basic_teeostream(std::basic_ostream<CharT, Traits> &o1, std::basic_ostream<CharT, Traits> &o2) :
       std::basic_ostream<CharT, Traits>(&tbuf), tbuf(o1.rdbuf(), o2.rdbuf()) {}
private:
    basic_teebuf<CharT, Traits> tbuf;
};
#endif
