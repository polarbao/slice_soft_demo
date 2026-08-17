#include "HostRipSafety.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace
{
std::filesystem::path FsPath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

QString QtPath(const std::filesystem::path& path)
{
#ifdef Q_OS_WIN
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

bool ComponentEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
#ifdef Q_OS_WIN
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
#else
    return left == right;
#endif
}

bool PathsEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
    auto leftPart = left.begin();
    auto rightPart = right.begin();
    for (; leftPart != left.end() && rightPart != right.end();
         ++leftPart, ++rightPart)
    {
        if (!ComponentEqual(*leftPart, *rightPart))
        {
            return false;
        }
    }
    return leftPart == left.end() && rightPart == right.end();
}

bool IsContained(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end()
            || !ComponentEqual(*rootPart, *candidatePart))
        {
            return false;
        }
    }
    return candidatePart != candidate.end();
}

bool IsReparsePoint(const std::filesystem::path& path)
{
    std::error_code error;
    if (std::filesystem::is_symlink(
            std::filesystem::symlink_status(path, error)))
    {
        return true;
    }
#ifdef Q_OS_WIN
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
    return false;
#endif
}

bool CanonicalAbsolute(
    const QString& value,
    std::filesystem::path* output)
{
    const std::filesystem::path path = FsPath(value);
    if (!path.is_absolute())
    {
        return false;
    }
    std::error_code error;
    *output = std::filesystem::weakly_canonical(path, error).lexically_normal();
    return !error;
}

QByteArray FileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
    {
        return {};
    }
    return hash.result();
}

bool CaptureOne(
    const std::filesystem::path& package,
    const QString& sourcePath,
    hostripsourcefileidentity* identity,
    QString* error)
{
    const std::filesystem::path original = FsPath(sourcePath);
    std::filesystem::path canonical;
    if (!CanonicalAbsolute(sourcePath, &canonical)
        || !IsContained(package, canonical)
        || IsReparsePoint(original))
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 源文件路径越界或包含链接：%1")
                         .arg(sourcePath);
        }
        return false;
    }
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(canonical, filesystemError)
        || filesystemError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 源文件不存在或不是普通文件：%1")
                         .arg(sourcePath);
        }
        return false;
    }
    const QString canonicalPath = QtPath(canonical);
    const QFileInfo info(canonicalPath);
    const QByteArray sha256 = FileSha256(canonicalPath);
    if (sha256.size() != 32)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("无法计算 RIP 源文件 SHA-256：%1")
                         .arg(sourcePath);
        }
        return false;
    }
    identity->path = canonicalPath;
    identity->size = info.size();
    identity->sha256 = sha256;
    return true;
}
}

bool HostRipSafety::CaptureSourceIdentity(
    const QString& packageDirectory,
    const QStringList& sourcePaths,
    QVector<hostripsourcefileidentity>* identity,
    QString* error)
{
    if (identity == nullptr || sourcePaths.isEmpty())
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 源文件身份请求为空。");
        }
        return false;
    }
    identity->clear();
    std::filesystem::path package;
    const std::filesystem::path originalPackage = FsPath(packageDirectory);
    std::error_code filesystemError;
    if (!CanonicalAbsolute(packageDirectory, &package)
        || IsReparsePoint(originalPackage)
        || !std::filesystem::is_directory(package, filesystemError)
        || filesystemError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP 源 Package 必须是无链接的真实目录。");
        }
        return false;
    }
    identity->reserve(sourcePaths.size());
    for (const QString& path : sourcePaths)
    {
        hostripsourcefileidentity item;
        if (!CaptureOne(package, path, &item, error))
        {
            identity->clear();
            return false;
        }
        identity->push_back(std::move(item));
    }
    return true;
}

bool HostRipSafety::VerifySourceIdentity(
    const QString& packageDirectory,
    const QVector<hostripsourcefileidentity>& expected,
    QString* error)
{
    QStringList paths;
    paths.reserve(expected.size());
    for (const hostripsourcefileidentity& item : expected)
    {
        paths.push_back(item.path);
    }
    QVector<hostripsourcefileidentity> actual;
    if (!CaptureSourceIdentity(packageDirectory, paths, &actual, error)
        || actual.size() != expected.size())
    {
        return false;
    }
    for (int index = 0; index < expected.size(); ++index)
    {
        if (actual.at(index).path != expected.at(index).path
            || actual.at(index).size != expected.at(index).size
            || actual.at(index).sha256 != expected.at(index).sha256)
        {
            if (error != nullptr)
            {
                *error = QStringLiteral("RIP 运行期间源切片包发生变化：%1")
                             .arg(expected.at(index).path);
            }
            return false;
        }
    }
    return true;
}

bool HostRipSafety::RemoveOwnedStaging(
    const QString& packageDirectory,
    const QString& stagingDirectory,
    QString* error)
{
    if (packageDirectory.isEmpty() || stagingDirectory.isEmpty())
    {
        return true;
    }
    const std::filesystem::path originalPackage = FsPath(packageDirectory);
    const std::filesystem::path originalStage = FsPath(stagingDirectory);
    std::filesystem::path package;
    std::filesystem::path stage;
    if (!CanonicalAbsolute(packageDirectory, &package)
        || !CanonicalAbsolute(stagingDirectory, &stage)
        || IsReparsePoint(originalPackage)
        || IsReparsePoint(originalStage)
        || !PathsEqual(stage.parent_path(), package)
        || !stage.filename().string().starts_with(".rip.staging.")
        || stage.filename().string().size() <= sizeof(".rip.staging.") - 1U)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral(
                "拒绝清理：RIP staging 不是无链接的 Package 直属目录。");
        }
        return false;
    }
    std::error_code filesystemError;
    const bool stageExists = std::filesystem::exists(stage, filesystemError);
    if (filesystemError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("拒绝清理：无法确认 RIP staging 身份。");
        }
        return false;
    }
    if (!stageExists)
    {
        return true;
    }
    if (!std::filesystem::is_directory(stage, filesystemError)
        || filesystemError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("拒绝清理：RIP staging 不是目录。");
        }
        return false;
    }
    for (std::filesystem::recursive_directory_iterator iterator(
             stage, std::filesystem::directory_options::none, filesystemError),
         end;
         !filesystemError && iterator != end;
         iterator.increment(filesystemError))
    {
        if (IsReparsePoint(iterator->path()))
        {
            if (error != nullptr)
            {
                *error = QStringLiteral(
                    "拒绝清理：RIP staging 内含链接或重解析点：%1")
                             .arg(QtPath(iterator->path()));
            }
            return false;
        }
    }
    if (filesystemError)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("拒绝清理：无法安全枚举 RIP staging。");
        }
        return false;
    }
    (void)std::filesystem::remove_all(stage, filesystemError);
    const bool remains = std::filesystem::exists(stage, filesystemError);
    if (filesystemError || remains)
    {
        if (error != nullptr)
        {
            *error = QStringLiteral("RIP staging 清理失败。");
        }
        return false;
    }
    return true;
}
