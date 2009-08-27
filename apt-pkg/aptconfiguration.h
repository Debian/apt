// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
/** \class APT::Configuration
 *  \brief Provide access methods to various configuration settings
 *
 *  This class and their methods providing a layer around the usual access
 *  methods with _config to ensure that settings are correct and to be able
 *  to set defaults without the need to recheck it in every method again.
 */
									/*}}}*/
#ifndef APT_CONFIGURATION_H
#define APT_CONFIGURATION_H
// Include Files							/*{{{*/
#include <string>
#include <vector>
									/*}}}*/
namespace APT {
class Configuration {							/*{{{*/
public:									/*{{{*/
	/** \brief Returns a vector of usable Compression Types
	 *
	 *  Files can be compressed in various ways to decrease the size of the
	 *  download. Therefore the Acquiremethods support a few compression types
	 *  and some archives provide also a few different types. This option
	 *  group exists to give the user the choice to prefer one type over the
	 *  other (some compression types are very resource intensive - great if you
	 *  have a limited download, bad if you have a really lowpowered hardware.)
	 *
	 *  This method ensures that the defaults are set and checks at runtime
	 *  if the type can be used. E.g. the current default is to prefer bzip2
	 *  over lzma and gz - if the bzip2 binary is not available it has not much
	 *  sense in downloading the bz2 file, therefore we will not return bz2 as
	 *  a usable compression type. The availability is checked with the settings
	 *  in the Dir::Bin group.
	 *
	 *  \param Cached saves the result so we need to calculated it only once
	 *                this parameter should ony be used for testing purposes.
	 *
	 *  \return a vector of (all) Language Codes in the prefered usage order
	 */
	std::vector<std::string> static const getCompressionTypes(bool const &Cached = true);
									/*}}}*/
};
									/*}}}*/
}
#endif
