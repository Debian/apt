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
#include <apt-pkg/macros.h>
#include <limits>
#include <string>
#include <vector>
									/*}}}*/
namespace APT {
namespace Configuration {							/*{{{*/
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
	 *                this parameter should only be used for testing purposes.
	 *
	 *  \return a vector of the compression types in the preferred usage order
	 */
	APT_PUBLIC std::vector<std::string> const getCompressionTypes(bool const &Cached = true);

	/** \brief Returns a vector of Language Codes
	 *
	 *  Languages can be defined with their two or five chars long code.
	 *  This methods handles the various ways to set the preferred codes,
	 *  honors the environment and ensures that the codes are not listed twice.
	 *
	 *  The special word "environment" will be replaced with the long and the short
	 *  code of the local settings and it will be insured that this will not add
	 *  duplicates. So in an german local the setting "environment, de_DE, en, de"
	 *  will result in "de_DE, de, en".
	 *
	 *  Another special word is "none" which separates the preferred from all codes
	 *  in this setting. So setting and method can be used to get codes the user want
	 *  to see or to get all language codes APT (should) have Translations available.
	 *
	 *  \param All return all codes or only codes for languages we want to use
	 *  \param Cached saves the result so we need to calculated it only once
	 *                this parameter should only be used for testing purposes.
	 *  \param Locale don't get the locale from the system but use this one instead
	 *                this parameter should only be used for testing purposes.
	 *
	 *  \return a vector of (all) Language Codes in the preferred usage order
	 */
	APT_PUBLIC std::vector<std::string> const getLanguages(bool const &All = false,
			bool const &Cached = true, char const ** const Locale = 0);

	/** \brief Are we interested in the given Language?
	 *
	 *  \param Lang is the language we want to check
	 *  \param All defines if we check against all codes or only against used codes
	 *  \return true if we are interested, false otherwise
	 */
	APT_PUBLIC bool checkLanguage(std::string Lang, bool const All = false);

	/** \brief Returns a vector of Architectures we support
	 *
	 *  \param Cached saves the result so we need to calculated it only once
	 *                this parameter should only be used for testing purposes.
	 *
	 *  \return a vector of Architectures in preferred order
	 */
	APT_PUBLIC std::vector<std::string> const getArchitectures(bool const &Cached = true);

	/** \brief Are we interested in the given Architecture?
	 *
	 *  \param Arch we want to check
	 *  \return true if we are interested, false otherwise
	 */
	APT_PUBLIC bool checkArchitecture(std::string const &Arch);

	/** \brief Representation of supported compressors */
	struct APT_PUBLIC Compressor {
		std::string Name;
		std::string Extension;
		std::string Binary;
		std::vector<std::string> CompressArgs;
		std::vector<std::string> UncompressArgs;
		unsigned short Cost;

		Compressor(char const *name, char const *extension, char const *binary,
			   char const *compressArg, char const *uncompressArg,
			   unsigned short const cost);
		Compressor() : Cost(std::numeric_limits<unsigned short>::max()) {};
	};

	/** \brief Return a vector of Compressors supported for data.tar's
	 *
	 *  \param Cached saves the result so we need to calculated it only once
	 *                this parameter should only be used for testing purposes.
	 *
	 *  \return a vector of Compressors
	 */
	APT_PUBLIC std::vector<Compressor> const getCompressors(bool const Cached = true);

	/** \brief Return a vector of extensions supported for data.tar's */
	APT_PUBLIC std::vector<std::string> const getCompressorExtensions();

	/** \return Return a vector of enabled build profile specifications */
	APT_PUBLIC std::vector<std::string> const getBuildProfiles();
	/** \return Return a comma-separated list of enabled build profile specifications */
	APT_PUBLIC std::string const getBuildProfilesString();

	std::string const getMachineID();

#ifdef APT_COMPILING_APT
	/** \return Whether we are running in a chroot */
	APT_PUBLIC bool isChroot();
	/** \return Check usr is merged or produce error. */
	APT_PUBLIC bool checkUsrMerged();
#endif
	/*}}}*/
}
									/*}}}*/
}
#endif
