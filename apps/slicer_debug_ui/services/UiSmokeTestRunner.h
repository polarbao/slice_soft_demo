#pragma once

#include <QString>

struct UiSmokeTestOptions {
    QString repo_root;
    QString case_name;
    QString config_path;
    QString package_path;
    QString package_a_path;
    QString package_b_path;
    QString output_path;
    bool yes{false};
};

class UiSmokeTestRunner {
public:
    int run(const UiSmokeTestOptions& options);

private:
    QString absoluteFromRepo(const UiSmokeTestOptions& options, const QString& path) const;
    int startup(const UiSmokeTestOptions& options);
    int loadPackage(const UiSmokeTestOptions& options);
    int saveAsConfig(const UiSmokeTestOptions& options);
    int chartLoad(const UiSmokeTestOptions& options);
    int overlayLoad(const UiSmokeTestOptions& options);
    int compareProfiles(const UiSmokeTestOptions& options);
    int fail(const QString& message) const;
    int pass(const QString& message) const;
};
