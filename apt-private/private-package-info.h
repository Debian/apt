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

//Sort kinds
bool OrderByStatus (PackageInfo &a, PackageInfo &b);
bool OrderByVersion (PackageInfo &a, PackageInfo &b);
bool OrderByAlphabetic (PackageInfo &a, PackageInfo &b);
bool OrderByReverseAlphabetic (PackageInfo &a, PackageInfo &b);

#endif
