#ifndef APT_PRIVATE_PACKAGE_INFO_H
#define APT_PRIVATE_PACKAGE_INFO_H

class PackageInfo
{
public:

    typedef enum{
        UNINSTALLED,
        INSTALLED_UPGRADABLE,
        INSTALLED_LOCAL,
        INSTALLED_AUTO_REMOVABLE,
        INSTALLED_AUTOMATIC,
        INSTALLED,
        UPGRADABLE,
        RESIDUAL_CONFIG
    } PackageStatus;

    typedef enum{
        ALPHABETIC,
        REVERSEALPHABETIC,
        STATUS,
        VERSION
    } SortBy;

    PackageInfo(pkgCacheFile &CacheFile, pkgRecords &records,
                       pkgCache::VerIterator const &V, std::string formated_output="");

    std::string format() const {return _format;}
    std::string name() const {return _name;}
    std::string version() const {return _version;}
    PackageStatus status() const {return _status;}
    std::string formated_output() const {return _formated_output;}

    void set_formated_output(const std::string& formated_output){_formated_output = formated_output;}
    void set_format(const std::string& format){_format = format;}

private:
    std::string  _name, 
            _formated_output, 
            _description, 
            _version, 
            _format = "${db::Status-Abbrev} ${Package} ${Version} ${Origin} ${Description}";
    PackageStatus _status;        

    PackageStatus GetPackageStatus(pkgCacheFile &CacheFile, pkgCache::VerIterator const &V);
};

PackageInfo::SortBy hashit (std::string inString);
//Sort kinds
bool OrderByStatus (const PackageInfo &a, const PackageInfo &b);
bool OrderByVersion (const PackageInfo &a, const PackageInfo &b);
bool OrderByAlphabetic (const PackageInfo &a, const PackageInfo &b);
bool OrderByReverseAlphabetic (const PackageInfo &a, const PackageInfo &b);

#endif
