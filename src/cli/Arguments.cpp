#include "cli/Arguments.hpp"

#include <charconv>
#include <sstream>

namespace asset_discovery::cli {
namespace {

std::optional<int> parsePositiveInteger(const std::string& value)
{
    int parsed = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end || parsed <= 0) {
        return std::nullopt;
    }
    return parsed;
}

bool needsValue(const std::string& option, std::size_t index, std::size_t size)
{
    return option.rfind("--", 0) == 0 && index + 1 >= size;
}

} // namespace

ParseResult parseArguments(const std::vector<std::string>& args)
{
    Options options;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];

        if (arg == "--help" || arg == "-h") {
            options.helpRequested = true;
            return {options, std::nullopt};
        }

        if (arg == "--pcap") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--pcap cần đường dẫn file"};
            }
            options.pcapPath = args[++i];
            continue;
        }

        if (arg == "--interface") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--interface cần tên interface"};
            }
            options.interfaceName = args[++i];
            continue;
        }

        if (arg == "--duration") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--duration cần số giây dương"};
            }
            const auto parsed = parsePositiveInteger(args[++i]);
            if (!parsed.has_value()) {
                return {options, "--duration phải là số nguyên dương"};
            }
            options.durationSeconds = *parsed;
            continue;
        }

        if (arg == "--filter") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--filter cần biểu thức BPF"};
            }
            const auto value = args[++i];
            if (value.empty()) {
                return {options, "--filter không được rỗng"};
            }
            options.packetFilter = value;
            continue;
        }

        if (arg == "--output") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--output cần một trong các giá trị: table, json, csv"};
            }
            const auto value = args[++i];
            if (value == "table") {
                options.outputFormat = OutputFormat::Table;
            } else if (value == "json") {
                options.outputFormat = OutputFormat::Json;
            } else if (value == "csv") {
                options.outputFormat = OutputFormat::Csv;
            } else {
                return {options, "định dạng xuất '" + value + "' không được hỗ trợ; cần một trong các giá trị: table, json, csv"};
            }
            continue;
        }

        if (arg == "--db-url") {
            if (needsValue(arg, i, args.size())) {
                return {options, "--db-url cần PostgreSQL connection string"};
            }
            const auto value = args[++i];
            if (value.empty()) {
                return {options, "--db-url không được rỗng; hãy dùng DATABASE_URL trong .env hoặc truyền connection string hợp lệ"};
            }
            options.databaseUrl = value;
            continue;
        }

        return {options, "tham số không xác định '" + arg + "'"};
    }

    if (options.pcapPath.has_value() == options.interfaceName.has_value()) {
        return {options, "chỉ cung cấp đúng một nguồn đầu vào: --pcap <file> hoặc --interface <name>"};
    }

    if (options.interfaceName.has_value() && !options.durationSeconds.has_value()) {
        return {options, "--interface cần --duration <seconds>"};
    }

    return {options, std::nullopt};
}

std::string usageText(const std::string& executableName)
{
    std::ostringstream output;
    output << "Cách dùng:\n"
           << "  " << executableName << " --pcap <file> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]\n"
           << "  " << executableName << " --interface <name> --duration <seconds> [--filter <bpf>] [--output table|json|csv] [--db-url <url>]\n"
           << "\nTùy chọn:\n"
           << "  --pcap <file>              Đọc packet từ file PCAP.\n"
           << "  --interface <name>         Capture packet từ interface đang chạy.\n"
           << "  --duration <seconds>       Thời lượng capture packet; bắt buộc khi dùng --interface.\n"
           << "  --filter <bpf>             Lọc packet bằng biểu thức BPF, ví dụ: arp or udp port 67 or udp port 68.\n"
           << "  --output table|json|csv    Định dạng xuất. Mặc định là table.\n"
           << "  --db-url <url>             Ghi asset vào PostgreSQL bằng psql client.\n"
           << "  -h, --help                 Hiển thị hướng dẫn này.\n";
    return output.str();
}

std::string outputFormatName(OutputFormat format)
{
    switch (format) {
    case OutputFormat::Table:
        return "table";
    case OutputFormat::Json:
        return "json";
    case OutputFormat::Csv:
        return "csv";
    }
    return "table";
}

} // namespace asset_discovery::cli
