#include "platform/ddcbackend.h"

#include <QByteArray>
#include <ddcutil_c_api.h>

namespace brightless::ddc {
namespace {
QString errorText(DDCA_Status status)
{
    const auto *text = ddca_rc_desc(status);
    return text ? QString::fromLocal8Bit(text) : QString::number(status);
}
}

std::vector<Device> enumerate(QString &error)
{
    // libddcutil caches detection; previous references are drained before this call.
    static bool detected = false;
    if (detected) {
        const auto status = ddca_redetect_displays();
        if (status != 0) {
            error = errorText(status);
            return {};
        }
    }
    detected = true;
    DDCA_Display_Info_List *raw = nullptr;
    const auto status = ddca_get_display_info_list2(false, &raw);
    const std::unique_ptr<DDCA_Display_Info_List, decltype(&ddca_free_display_info_list)>
        list(raw, &ddca_free_display_info_list);
    std::vector<Device> devices;
    if (status != 0) {
        error = errorText(status);
        return devices;
    }
    if (!list) {
        return devices;
    }
    for (int i = 0; i < list->ct; ++i) {
        const auto &info = list->info[i];
        const auto path = info.path.io_mode == DDCA_IO_I2C
            ? QStringLiteral("i2c-%1").arg(info.path.path.i2c_busno)
            : QStringLiteral("usb-%1").arg(info.path.path.hiddev_devno);
        // ponytail: EDID + bus distinguishes identical panels; use connector IDs if buses renumber.
        const auto id = QString::fromLatin1(QByteArray(
            reinterpret_cast<const char *>(info.edid_bytes), sizeof(info.edid_bytes)).toHex())
            + QLatin1Char(':') + path;
        auto name = QString::fromLatin1(info.model_name).trimmed();
        if (name.isEmpty()) {
            name = QStringLiteral("%1 %2").arg(QString::fromLatin1(info.mfg_id).trimmed())
                       .arg(info.product_code, 4, 16, QLatin1Char('0')).trimmed();
        }
        devices.push_back({id, name, {info.dref, [](void *) {}}, path});
    }
    return devices;
}

Values read(const Device &device, const Codes &codes)
{
    Values values;
    DDCA_Display_Handle handle = nullptr;
    if (ddca_open_display2(device.native.get(), false, &handle) != 0) {
        return values;
    }
    for (std::size_t i = 0; i < codes.size(); ++i) {
        DDCA_Non_Table_Vcp_Value value{};
        if (ddca_get_non_table_vcp_value(handle, codes[i], &value) == 0) {
            values[i] = Value{
                static_cast<std::uint16_t>((value.sh << 8) | value.sl),
                static_cast<std::uint16_t>((value.mh << 8) | value.ml)};
        }
        if (i == 0 && (!values[0] || values[0]->maximum == 0)) break;
    }
    ddca_close_display(handle);
    return values;
}

QString write(const Device &device, const Writes &values)
{
    DDCA_Display_Handle handle = nullptr;
    auto status = ddca_open_display2(device.native.get(), false, &handle);
    if (status != 0) {
        return errorText(status);
    }
    QString error;
    for (const auto &[code, value] : values) {
        status = ddca_set_non_table_vcp_value(handle, code, value >> 8, value & 0xff);
        if (status != 0 && error.isEmpty()) {
            error = QStringLiteral("VCP 0x%1: %2").arg(code, 2, 16, QLatin1Char('0'))
                        .arg(errorText(status));
        }
    }
    ddca_close_display(handle);
    return error;
}

} // namespace brightless::ddc
