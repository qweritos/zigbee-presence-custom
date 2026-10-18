"""Configuration entities for the open Tuya MCU radio bridge (no automation)."""
from zigpy.quirks import CustomCluster
import zigpy.types as t
from zigpy.zcl import foundation
from zigpy.zcl import ClusterType
from zigpy.zcl.clusters.general import OnOff
from zhaquirks.builder import EntityType, QuirkBuilder, ReportingConfig


class DatapointType(t.enum8):
    Boolean = 1
    Integer = 2
    Enum = 4
    Bitmap = 5


class DirectOnOffClient(CustomCluster, OnOff):
    """Peer control needs no automatic On/Off binding to the coordinator."""

    async def bind(self, **kwargs):
        # ZHA configuration normally binds a remote's client to its coordinator.
        # Explicit peer bindings use ZDO and remain fully supported.
        self.debug("Skipping automatic coordinator On/Off binding")
        return (foundation.Status.SUCCESS,)


class RouteCluster(CustomCluster):
    cluster_id = 0xFC10
    name = "MCU binding route"
    ep_attribute = "mcu_binding_route"

    class AttributeDefs(foundation.BaseAttributeDefs):
        enabled = foundation.ZCLAttributeDef(id=0, type=t.Bool, access="rw", is_manufacturer_specific=False)
        datapoint = foundation.ZCLAttributeDef(id=1, type=t.uint8_t, access="rw", is_manufacturer_specific=False)
        datapoint_type = foundation.ZCLAttributeDef(id=2, type=t.enum8, access="rw", is_manufacturer_specific=False)
        active_value = foundation.ZCLAttributeDef(id=3, type=t.uint32_t, access="rw", is_manufacturer_specific=False)
        clear_value = foundation.ZCLAttributeDef(id=4, type=t.uint32_t, access="rw", is_manufacturer_specific=False)
        off_delay = foundation.ZCLAttributeDef(id=5, type=t.uint16_t, access="rw", is_manufacturer_specific=False)
        input_value = foundation.ZCLAttributeDef(id=0x10, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        input_known = foundation.ZCLAttributeDef(id=0x11, type=t.Bool, access="r", is_manufacturer_specific=False)
        binding_count = foundation.ZCLAttributeDef(id=0x12, type=t.uint8_t, access="r", is_manufacturer_specific=False)
        matched_reports = foundation.ZCLAttributeDef(id=0x13, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        ignored_values = foundation.ZCLAttributeDef(id=0x14, type=t.uint32_t, access="r", is_manufacturer_specific=False)


class DiagnosticsCluster(CustomCluster):
    cluster_id = 0xFC11
    name = "MCU bridge diagnostics"
    ep_attribute = "mcu_bridge_diagnostics"

    class AttributeDefs(foundation.BaseAttributeDefs):
        frames = foundation.ZCLAttributeDef(id=0, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        bad_checksums = foundation.ZCLAttributeDef(id=1, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        queued_commands = foundation.ZCLAttributeDef(id=3, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        delivery_failures = foundation.ZCLAttributeDef(id=4, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        persistence_failures = foundation.ZCLAttributeDef(id=5, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        last_datapoint = foundation.ZCLAttributeDef(id=6, type=t.uint8_t, access="r", is_manufacturer_specific=False)
        last_datapoint_type = foundation.ZCLAttributeDef(id=7, type=t.enum8, access="r", is_manufacturer_specific=False)
        last_datapoint_value = foundation.ZCLAttributeDef(id=8, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        mcu_version = foundation.ZCLAttributeDef(id=9, type=t.uint8_t, access="r", is_manufacturer_specific=False)
        delivered_commands = foundation.ZCLAttributeDef(id=10, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        serial_tx_dropped = foundation.ZCLAttributeDef(id=11, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        datapoint_reports = foundation.ZCLAttributeDef(id=12, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        rejected_versions = foundation.ZCLAttributeDef(id=13, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        frame_timeouts = foundation.ZCLAttributeDef(id=14, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        illuminance_datapoint = foundation.ZCLAttributeDef(id=16, type=t.uint8_t, access="rw", is_manufacturer_specific=False)
        uart_bytes = foundation.ZCLAttributeDef(id=32, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        uart_overflows = foundation.ZCLAttributeDef(id=33, type=t.uint32_t, access="r", is_manufacturer_specific=False)
        uart_errors = foundation.ZCLAttributeDef(id=34, type=t.uint32_t, access="r", is_manufacturer_specific=False)


class Ztc6RadarCluster(CustomCluster):
    cluster_id = 0xFC20
    name = "ZTC6 radar settings"
    ep_attribute = "ztc6_radar_settings"

    def _update_attribute(self, attrid, value, *args, **kwargs):
        # Tuya products need not report every optional DP. Preserve unknown state
        # rather than showing an unreported boolean as On or uint32 as a number.
        if (attrid == 101 and value not in (0, 1)) or (
            attrid in (2, 3, 4, 102) and value == 0xFFFFFFFF
        ):
            value = None
        return super()._update_attribute(attrid, value, *args, **kwargs)

    class AttributeDefs(foundation.BaseAttributeDefs):
        motion_sensitivity = foundation.ZCLAttributeDef(id=2, type=t.uint32_t, access="rw", is_manufacturer_specific=False)
        minimum_range = foundation.ZCLAttributeDef(id=3, type=t.uint32_t, access="rw", is_manufacturer_specific=False)
        maximum_range = foundation.ZCLAttributeDef(id=4, type=t.uint32_t, access="rw", is_manufacturer_specific=False)
        distance = foundation.ZCLAttributeDef(id=9, type=t.Single, access="r", is_manufacturer_specific=False)
        led = foundation.ZCLAttributeDef(id=101, type=t.Bool, access="rw", is_manufacturer_specific=False)
        presence_sensitivity = foundation.ZCLAttributeDef(id=102, type=t.uint32_t, access="rw", is_manufacturer_specific=False)


def build_quirk(model: str, radar: bool = False):
    builder = QuirkBuilder("OpenZigbee-MCU", model).replaces(DiagnosticsCluster)
    for ep in range(1, 5):
        builder.replaces(RouteCluster, endpoint_id=ep)
        builder.replaces(DirectOnOffClient, endpoint_id=ep, cluster_type=ClusterType.Client)
        builder.switch("enabled", RouteCluster.cluster_id, endpoint_id=ep,
                       fallback_name=f"Route {ep} enabled", translation_key="route_enabled")
        builder.enum("datapoint_type", DatapointType, RouteCluster.cluster_id, endpoint_id=ep,
                     fallback_name=f"Route {ep} DP type", translation_key="route_datapoint_type")
        for attr, maximum, label in [
            ("datapoint", 255, "datapoint"),
            ("active_value", 4294967295, "active value"),
            ("clear_value", 4294967295, "clear value"), ("off_delay", 65535, "off delay"),
        ]:
            builder.number(attr, RouteCluster.cluster_id, endpoint_id=ep,
                           min_value=0, max_value=maximum, step=1,
                           unit="s" if attr == "off_delay" else None,
                           fallback_name=f"Route {ep} {label}", translation_key=f"route_{attr}")
        for attr in ["input_value", "binding_count", "matched_reports", "ignored_values"]:
            builder.sensor(attr, RouteCluster.cluster_id, endpoint_id=ep,
                           entity_type=EntityType.DIAGNOSTIC, initially_disabled=True,
                           fallback_name=f"Route {ep} {attr.replace('_', ' ')}", translation_key=f"route_{attr}")
    builder.number("illuminance_datapoint", DiagnosticsCluster.cluster_id, min_value=0,
                   max_value=255, step=1, fallback_name="Illuminance datapoint", translation_key="illuminance_datapoint")
    for attr in ["frames", "bad_checksums", "queued_commands", "delivered_commands",
                 "delivery_failures", "persistence_failures", "uart_overflows", "uart_errors"]:
        builder.sensor(attr, DiagnosticsCluster.cluster_id, entity_type=EntityType.DIAGNOSTIC,
                       initially_disabled=True, fallback_name=attr.replace('_', ' ').capitalize(), translation_key=attr)
    if radar:
        builder.replaces(Ztc6RadarCluster)
        builder.switch("led", Ztc6RadarCluster.cluster_id, fallback_name="Radar LED", translation_key="radar_led")
        for attr, low, high, multiplier, unit, name in [
            ("motion_sensitivity", 1, 10, 1, None, "Motion sensitivity"),
            ("presence_sensitivity", 0, 100, 0.1, None, "Presence sensitivity"),
            ("minimum_range", 0, 950, 0.01, "m", "Minimum range"),
            ("maximum_range", 0, 950, 0.01, "m", "Maximum range"),
        ]:
            builder.number(attr, Ztc6RadarCluster.cluster_id, min_value=low, max_value=high,
                           step=1, multiplier=multiplier, unit=unit, fallback_name=name, translation_key=attr)
        builder.sensor("distance", Ztc6RadarCluster.cluster_id, divisor=100, unit="m",
                       fallback_name="Target distance", translation_key="target_distance",
                       reporting_config=ReportingConfig(min_interval=5, max_interval=300, reportable_change=5))
    return builder.add_to_registry()


GENERIC_QUIRK = build_quirk("TuyaMCU-Bridge")
RADAR_QUIRK = build_quirk("TuyaMCU-Bridge-ZTC6", radar=True)
