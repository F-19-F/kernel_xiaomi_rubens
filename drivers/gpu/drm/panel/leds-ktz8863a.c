// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

#define KTZ8863A_DISP_REV		0x01
#define KTZ8863A_DISP_BC1		0x02
#define KTZ8863A_DISP_BC2		0x03
#define KTZ8863A_DISP_BB_LSB		0x04
#define KTZ8863A_DISP_BB_MSB		0x05
#define KTZ8863A_DISP_BAFLT		0x06
#define KTZ8863A_DISP_BAFHT		0x07
#define KTZ8863A_DISP_BL_ENABLE	        0x08
#define KTZ8863A_DISP_BIAS_CONF1	0x09
#define KTZ8863A_DISP_BIAS_CONF2	0x0a
#define KTZ8863A_DISP_BIAS_CONF3	0x0b
#define KTZ8863A_DISP_BIAS_BOOST	0x0c
#define KTZ8863A_DISP_BIAS_VPOS	        0x0d
#define KTZ8863A_DISP_BIAS_VNEG	        0x0e
#define KTZ8863A_DISP_FLAGS		0x0f
#define KTZ8863A_DISP_OPTION1	        0x10
#define KTZ8863A_DISP_OPTION2	        0x11
#define KTZ8863A_DISP_PTD_LSB	        0x12
#define KTZ8863A_DISP_PTD_MSB	        0x13

#define BL_LEVEL_MAX 2047
#define BL_HBM_L1 1913/* 19.972mA for exponential mapping*/
#define BL_HBM_L2 1952/* 22.483mA for exponential mapping*/
#define BL_HBM_L3 1987/* 25.004mA for exponential mapping*/

/**
 * struct ktz8863a_led -
 * @lock - Lock for reading/writing the device
 * @client - Pointer to the I2C client
 * @level - setting backlight level
**/
struct ktz8863a_led {
	struct mutex lock;
	struct i2c_client *client;
	int level;
	int hbm_on;
};

struct ktz8863a_reg {
	uint8_t reg;
	uint8_t value;
};

static struct ktz8863a_led g_ktz8863a_led;
static struct ktz8863a_reg ktz8863a_regs_conf[] = {
	{ KTZ8863A_DISP_BC1, 0x38 },/* disable pwm*/
	{ KTZ8863A_DISP_BC2, 0x85 },/* disable dimming*/
	{ KTZ8863A_DISP_BIAS_BOOST, 0x24 },/* set LCM_OUT voltage*/
	{ KTZ8863A_DISP_BIAS_VPOS, 0x1e },/* set vsp to +5.5V*/
	{ KTZ8863A_DISP_BIAS_VNEG, 0x1e },/* set vsn to -5.5V*/
};

static int bl_level_remap[BL_LEVEL_MAX+1] = {
0, 2, 3, 4, 5, 6, 7, 8, 52, 91, 126, 157, 186, 212, 237, 259,
281, 301, 320, 337, 354, 370, 386, 400, 414, 428, 441, 453, 465, 477, 488, 499,
509, 519, 529, 539, 548, 557, 566, 574, 583, 591, 599, 606, 614, 621, 629, 636,
643, 649, 656, 663, 669, 675, 681, 687, 693, 699, 705, 711, 716, 722, 727, 732,
737, 742, 747, 752, 757, 762, 767, 772, 776, 781, 785, 790, 794, 798, 803, 807,
811, 815, 819, 823, 827, 831, 835, 838, 842, 846, 850, 853, 857, 860, 864, 867,
871, 874, 878, 881, 884, 888, 891, 894, 897, 900, 904, 907, 910, 913, 916, 919,
922, 925, 927, 930, 933, 936, 939, 942, 944, 947, 950, 953, 955, 958, 960, 963,
966, 968, 971, 973, 976, 978, 981, 983, 986, 988, 990, 993, 995, 998, 1000, 1002,
1004, 1007, 1009, 1011, 1013, 1016, 1018, 1020, 1022, 1024, 1027, 1029, 1031, 1033, 1035, 1037,
1039, 1041, 1043, 1045, 1047, 1049, 1051, 1053, 1055, 1057, 1059, 1061, 1063, 1065, 1067, 1069,
1071, 1072, 1074, 1076, 1078, 1080, 1082, 1083, 1085, 1087, 1089, 1091, 1092, 1094, 1096, 1097,
1099, 1101, 1103, 1104, 1106, 1108, 1109, 1111, 1113, 1114, 1116, 1118, 1119, 1121, 1122, 1124,
1126, 1127, 1129, 1130, 1132, 1133, 1135, 1136, 1138, 1140, 1141, 1143, 1144, 1146, 1147, 1149,
1150, 1151, 1153, 1154, 1156, 1157, 1159, 1160, 1162, 1163, 1164, 1166, 1167, 1169, 1170, 1171,
1173, 1174, 1175, 1177, 1178, 1179, 1181, 1182, 1184, 1185, 1186, 1187, 1189, 1190, 1191, 1193,
1194, 1195, 1197, 1198, 1199, 1200, 1202, 1203, 1204, 1205, 1207, 1208, 1209, 1210, 1212, 1213,
1214, 1215, 1216, 1218, 1219, 1220, 1221, 1222, 1223, 1225, 1226, 1227, 1228, 1229, 1230, 1232,
1233, 1234, 1235, 1236, 1237, 1238, 1240, 1241, 1242, 1243, 1244, 1245, 1246, 1247, 1248, 1249,
1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 1259, 1260, 1261, 1262, 1263, 1264, 1265, 1266,
1267, 1268, 1270, 1271, 1272, 1273, 1274, 1275, 1276, 1277, 1278, 1279, 1280, 1281, 1282, 1283,
1284, 1285, 1285, 1286, 1287, 1288, 1289, 1290, 1291, 1292, 1293, 1294, 1295, 1296, 1297, 1298,
1299, 1300, 1301, 1302, 1303, 1304, 1304, 1305, 1306, 1307, 1308, 1309, 1310, 1311, 1312, 1313,
1314, 1314, 1315, 1316, 1317, 1318, 1319, 1320, 1321, 1321, 1322, 1323, 1324, 1325, 1326, 1327,
1328, 1328, 1329, 1330, 1331, 1332, 1333, 1333, 1334, 1335, 1336, 1337, 1338, 1338, 1339, 1340,
1341, 1342, 1343, 1343, 1344, 1345, 1346, 1347, 1347, 1348, 1349, 1350, 1351, 1352, 1352, 1353,
1354, 1355, 1355, 1356, 1357, 1358, 1359, 1359, 1360, 1361, 1362, 1362, 1363, 1364, 1365, 1366,
1366, 1367, 1368, 1369, 1369, 1370, 1371, 1372, 1372, 1373, 1374, 1375, 1375, 1376, 1377, 1378,
1378, 1379, 1380, 1380, 1381, 1382, 1383, 1383, 1384, 1385, 1386, 1386, 1387, 1388, 1388, 1389,
1390, 1391, 1391, 1392, 1393, 1393, 1394, 1395, 1395, 1396, 1397, 1398, 1398, 1399, 1400, 1400,
1401, 1402, 1402, 1403, 1404, 1404, 1405, 1406, 1406, 1407, 1408, 1408, 1409, 1410, 1410, 1411,
1412, 1412, 1413, 1414, 1414, 1415, 1416, 1416, 1417, 1418, 1418, 1419, 1420, 1420, 1421, 1422,
1422, 1423, 1424, 1424, 1425, 1425, 1426, 1427, 1427, 1428, 1429, 1429, 1430, 1431, 1431, 1432,
1432, 1433, 1434, 1434, 1435, 1436, 1436, 1437, 1437, 1438, 1439, 1439, 1440, 1440, 1441, 1442,
1442, 1443, 1443, 1444, 1445, 1445, 1446, 1446, 1447, 1448, 1448, 1449, 1449, 1450, 1451, 1451,
1452, 1452, 1453, 1454, 1454, 1455, 1455, 1456, 1456, 1457, 1458, 1458, 1459, 1459, 1460, 1461,
1461, 1462, 1462, 1463, 1463, 1464, 1464, 1465, 1466, 1466, 1467, 1467, 1468, 1468, 1469, 1470,
1470, 1471, 1471, 1472, 1472, 1473, 1473, 1474, 1475, 1475, 1476, 1476, 1477, 1477, 1478, 1478,
1479, 1479, 1480, 1481, 1481, 1482, 1482, 1483, 1483, 1484, 1484, 1485, 1485, 1486, 1486, 1487,
1487, 1488, 1488, 1489, 1490, 1490, 1491, 1491, 1492, 1492, 1493, 1493, 1494, 1494, 1495, 1495,
1496, 1496, 1497, 1497, 1498, 1498, 1499, 1499, 1500, 1500, 1501, 1501, 1502, 1502, 1503, 1503,
1504, 1504, 1505, 1505, 1506, 1506, 1507, 1507, 1508, 1508, 1509, 1509, 1510, 1510, 1511, 1511,
1512, 1512, 1513, 1513, 1514, 1514, 1515, 1515, 1516, 1516, 1517, 1517, 1518, 1518, 1519, 1519,
1520, 1520, 1521, 1521, 1522, 1522, 1522, 1523, 1523, 1524, 1524, 1525, 1525, 1526, 1526, 1527,
1527, 1528, 1528, 1529, 1529, 1530, 1530, 1530, 1531, 1531, 1532, 1532, 1533, 1533, 1534, 1534,
1535, 1535, 1535, 1536, 1536, 1537, 1537, 1538, 1538, 1539, 1539, 1540, 1540, 1540, 1541, 1541,
1542, 1542, 1543, 1543, 1544, 1544, 1544, 1545, 1545, 1546, 1546, 1547, 1547, 1548, 1548, 1548,
1549, 1549, 1550, 1550, 1551, 1551, 1552, 1552, 1552, 1553, 1553, 1554, 1554, 1555, 1555, 1555,
1556, 1556, 1557, 1557, 1558, 1558, 1558, 1559, 1559, 1560, 1560, 1561, 1561, 1561, 1562, 1562,
1563, 1563, 1563, 1564, 1564, 1565, 1565, 1566, 1566, 1566, 1567, 1567, 1568, 1568, 1568, 1569,
1569, 1570, 1570, 1571, 1571, 1571, 1572, 1572, 1573, 1573, 1573, 1574, 1574, 1575, 1575, 1575,
1576, 1576, 1577, 1577, 1577, 1578, 1578, 1579, 1579, 1579, 1580, 1580, 1581, 1581, 1581, 1582,
1582, 1583, 1583, 1583, 1584, 1584, 1585, 1585, 1585, 1586, 1586, 1587, 1587, 1587, 1588, 1588,
1588, 1589, 1589, 1590, 1590, 1590, 1591, 1591, 1592, 1592, 1592, 1593, 1593, 1593, 1594, 1594,
1595, 1595, 1595, 1596, 1596, 1597, 1597, 1597, 1598, 1598, 1598, 1599, 1599, 1600, 1600, 1600,
1601, 1601, 1601, 1602, 1602, 1603, 1603, 1603, 1604, 1604, 1604, 1605, 1605, 1606, 1606, 1606,
1607, 1607, 1607, 1608, 1608, 1608, 1609, 1609, 1610, 1610, 1610, 1611, 1611, 1611, 1612, 1612,
1612, 1613, 1613, 1614, 1614, 1614, 1615, 1615, 1615, 1616, 1616, 1616, 1617, 1617, 1617, 1618,
1618, 1619, 1619, 1619, 1620, 1620, 1620, 1621, 1621, 1621, 1622, 1622, 1622, 1623, 1623, 1623,
1624, 1624, 1624, 1625, 1625, 1626, 1626, 1626, 1627, 1627, 1627, 1628, 1628, 1628, 1629, 1629,
1629, 1630, 1630, 1630, 1631, 1631, 1631, 1632, 1632, 1632, 1633, 1633, 1633, 1634, 1634, 1634,
1635, 1635, 1635, 1636, 1636, 1636, 1637, 1637, 1637, 1638, 1638, 1638, 1639, 1639, 1639, 1640,
1640, 1640, 1641, 1641, 1641, 1642, 1642, 1642, 1643, 1643, 1643, 1644, 1644, 1644, 1645, 1645,
1645, 1646, 1646, 1646, 1647, 1647, 1647, 1648, 1648, 1648, 1649, 1649, 1649, 1650, 1650, 1650,
1651, 1651, 1651, 1652, 1652, 1652, 1653, 1653, 1653, 1653, 1654, 1654, 1654, 1655, 1655, 1655,
1656, 1656, 1656, 1657, 1657, 1657, 1658, 1658, 1658, 1659, 1659, 1659, 1659, 1660, 1660, 1660,
1661, 1661, 1661, 1662, 1662, 1662, 1663, 1663, 1663, 1664, 1664, 1664, 1664, 1665, 1665, 1665,
1666, 1666, 1666, 1667, 1667, 1667, 1668, 1668, 1668, 1668, 1669, 1669, 1669, 1670, 1670, 1670,
1671, 1671, 1671, 1671, 1672, 1672, 1672, 1673, 1673, 1673, 1674, 1674, 1674, 1674, 1675, 1675,
1675, 1676, 1676, 1676, 1677, 1677, 1677, 1677, 1678, 1678, 1678, 1679, 1679, 1679, 1680, 1680,
1680, 1680, 1681, 1681, 1681, 1682, 1682, 1682, 1682, 1683, 1683, 1683, 1684, 1684, 1684, 1684,
1685, 1685, 1685, 1686, 1686, 1686, 1687, 1687, 1687, 1687, 1688, 1688, 1688, 1689, 1689, 1689,
1689, 1690, 1690, 1690, 1691, 1691, 1691, 1691, 1692, 1692, 1692, 1693, 1693, 1693, 1693, 1694,
1694, 1694, 1694, 1695, 1695, 1695, 1696, 1696, 1696, 1696, 1697, 1697, 1697, 1698, 1698, 1698,
1698, 1699, 1699, 1699, 1700, 1700, 1700, 1700, 1701, 1701, 1701, 1701, 1702, 1702, 1702, 1703,
1703, 1703, 1703, 1704, 1704, 1704, 1704, 1705, 1705, 1705, 1706, 1706, 1706, 1706, 1707, 1707,
1707, 1707, 1708, 1708, 1708, 1709, 1709, 1709, 1709, 1710, 1710, 1710, 1710, 1711, 1711, 1711,
1712, 1712, 1712, 1712, 1713, 1713, 1713, 1713, 1714, 1714, 1714, 1714, 1715, 1715, 1715, 1715,
 1716, 1716, 1716, 1717, 1717, 1717, 1717, 1718, 1718, 1718, 1718, 1719, 1719, 1719, 1719, 1720,
 1720, 1720, 1720, 1721, 1721, 1721, 1722, 1722, 1722, 1722, 1723, 1723, 1723, 1723, 1724, 1724,
 1724, 1724, 1725, 1725, 1725, 1725, 1726, 1726, 1726, 1726, 1727, 1727, 1727, 1727, 1728, 1728,
 1728, 1728, 1729, 1729, 1729, 1729, 1730, 1730, 1730, 1730, 1731, 1731, 1731, 1731, 1732, 1732,
 1732, 1732, 1733, 1733, 1733, 1733, 1734, 1734, 1734, 1734, 1735, 1735, 1735, 1735, 1736, 1736,
 1736, 1736, 1737, 1737, 1737, 1737, 1738, 1738, 1738, 1738, 1739, 1739, 1739, 1739, 1740, 1740,
 1740, 1740, 1741, 1741, 1741, 1741, 1742, 1742, 1742, 1742, 1743, 1743, 1743, 1743, 1744, 1744,
 1744, 1744, 1745, 1745, 1745, 1745, 1746, 1746, 1746, 1746, 1746, 1747, 1747, 1747, 1747, 1748,
 1748, 1748, 1748, 1749, 1749, 1749, 1749, 1750, 1750, 1750, 1750, 1751, 1751, 1751, 1751, 1751,
 1752, 1752, 1752, 1752, 1753, 1753, 1753, 1753, 1754, 1754, 1754, 1754, 1755, 1755, 1755, 1755,
 1755, 1756, 1756, 1756, 1756, 1757, 1757, 1757, 1757, 1758, 1758, 1758, 1758, 1759, 1759, 1759,
 1759, 1759, 1760, 1760, 1760, 1760, 1761, 1761, 1761, 1761, 1762, 1762, 1762, 1762, 1762, 1763,
 1763, 1763, 1763, 1764, 1764, 1764, 1764, 1764, 1765, 1765, 1765, 1765, 1766, 1766, 1766, 1766,
 1767, 1767, 1767, 1767, 1767, 1768, 1768, 1768, 1768, 1769, 1769, 1769, 1769, 1769, 1770, 1770,
 1770, 1770, 1771, 1771, 1771, 1771, 1771, 1772, 1772, 1772, 1772, 1773, 1773, 1773, 1773, 1773,
 1774, 1774, 1774, 1774, 1775, 1775, 1775, 1775, 1775, 1776, 1776, 1776, 1776, 1777, 1777, 1777,
 1777, 1777, 1778, 1778, 1778, 1778, 1779, 1779, 1779, 1779, 1779, 1780, 1780, 1780, 1780, 1780,
 1781, 1781, 1781, 1781, 1782, 1782, 1782, 1782, 1782, 1783, 1783, 1783, 1783, 1784, 1784, 1784,
 1784, 1784, 1785, 1785, 1785, 1785, 1785, 1786, 1786, 1786, 1786, 1786, 1787, 1787, 1787, 1787,
 1788, 1788, 1788, 1788, 1788, 1789, 1789, 1789, 1789, 1789, 1790, 1790, 1790, 1790, 1791, 1791,
 1791, 1791, 1791, 1792, 1792, 1792, 1792, 1792, 1793, 1793, 1793, 1793, 1793, 1794, 1794, 1794,
 1794, 1794, 1795, 1795, 1795, 1795, 1796, 1796, 1796, 1796, 1796, 1797, 1797, 1797, 1797, 1797,
 1798, 1798, 1798, 1798, 1798, 1799, 1799, 1799, 1799, 1799, 1800, 1800, 1800, 1800, 1800, 1801,
 1801, 1801, 1801, 1801, 1802, 1802, 1802, 1802, 1802, 1803, 1803, 1803, 1803, 1804, 1804, 1804,
 1804, 1804, 1805, 1805, 1805, 1805, 1805, 1806, 1806, 1806, 1806, 1806, 1807, 1807, 1807, 1807,
 1807, 1808, 1808, 1808, 1808, 1808, 1809, 1809, 1809, 1809, 1809, 1810, 1810, 1810, 1810, 1810,
 1811, 1811, 1811, 1811, 1811, 1811, 1812, 1812, 1812, 1812, 1812, 1813, 1813, 1813, 1813, 1813,
 1814, 1814, 1814, 1814, 1814, 1815, 1815, 1815, 1815, 1815, 1816, 1816, 1816, 1816, 1816, 1817,
 1817, 1817, 1817, 1817, 1818, 1818, 1818, 1818, 1818, 1819, 1819, 1819, 1819, 1819, 1819, 1820,
 1820, 1820, 1820, 1820, 1821, 1821, 1821, 1821, 1821, 1822, 1822, 1822, 1822, 1822, 1823, 1823,
 1823, 1823, 1823, 1824, 1824, 1824, 1824, 1824, 1824, 1825, 1825, 1825, 1825, 1825, 1826, 1826,
 1826, 1826, 1826, 1827, 1827, 1827, 1827, 1827, 1827, 1828, 1828, 1828, 1828, 1828, 1829, 1829,
 1829, 1829, 1829, 1830, 1830, 1830, 1830, 1830, 1830, 1831, 1831, 1831, 1831, 1831, 1832, 1832,
 1832, 1832, 1832, 1833, 1833, 1833, 1833, 1833, 1833, 1834, 1834, 1834, 1834, 1834, 1835, 1835,
 1835, 1835, 1835, 1835, 1836, 1836, 1836, 1836, 1836, 1837, 1837, 1837, 1837, 1837, 1837, 1838,
 1838, 1838, 1838, 1838, 1839, 1839, 1839, 1839, 1839, 1839, 1840, 1840, 1840, 1840, 1840, 1841,
 1841, 1841, 1841, 1841, 1841, 1842, 1842, 1842, 1842, 1842, 1843, 1843, 1843, 1843, 1843, 1843,
 1844, 1844, 1844, 1844, 1844, 1845, 1845, 1845, 1845, 1845, 1845, 1846, 1846, 1846, 1846, 1846,
 1846, 1847, 1847, 1847, 1847, 1847, 1848, 1848, 1848, 1848, 1848, 1848, 1849, 1849, 1849, 1849,
 1849, 1849, 1850, 1850, 1850, 1850, 1850, 1851, 1851, 1851, 1851, 1851, 1851, 1852, 1852, 1852,
 1852, 1852, 1852, 1853, 1853, 1853, 1853, 1853, 1854, 1854, 1854, 1854, 1854, 1854, 1855, 1855,
 1855, 1855, 1855, 1855, 1856, 1856, 1856, 1856, 1856, 1856, 1857, 1857, 1857, 1857, 1857, 1857,
 1858, 1858, 1858, 1858, 1858, 1859, 1859, 1859, 1859, 1859, 1859, 1860, 1860, 1860, 1860, 1860,
 1860, 1861, 1861, 1861, 1861, 1861, 1861, 1862, 1862, 1862, 1862, 1862, 1862, 1863, 1863, 1863,
 1863, 1863, 1863, 1864, 1864, 1864, 1864, 1864, 1864, 1865, 1865, 1865, 1865, 1865, 1865, 1866,
 1866, 1866, 1866, 1866, 1866, 1867, 1867, 1867, 1867, 1867, 1867, 1868, 1868, 1868, 1868, 1868,
 1868, 1869, 1869, 1869, 1869, 1869, 1869, 1870, 1870, 1870, 1870, 1870, 1870, 1871, 1871, 1871,
 1871, 1871, 1871, 1872, 1872, 1872, 1872, 1872, 1872, 1873, 1873, 1873, 1873, 1873, 1873, 1874,
 1874, 1874, 1874, 1874, 1874, 1875, 1875, 1875, 1875, 1875, 1875, 1876, 1876, 1876, 1876, 1876,
 1876, 1876, 1877, 1877, 1877, 1877, 1877, 1877, 1878, 1878, 1878, 1878, 1878, 1878, 1879, 1879
};

int ktz8863a_reg_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = -EINVAL;
	char write_data[2] = { 0 };

	if (g_ktz8863a_led.client == NULL) {
		pr_info("ERROR!! ktz8863a i2c client is null\n");
		return ret;
	}

	if (addr < KTZ8863A_DISP_REV || addr > KTZ8863A_DISP_PTD_MSB) {
		pr_info("ERROR!! ktz8863a addr overflow\n");
		return ret;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(g_ktz8863a_led.client, write_data, 2);
	if (ret < 0)
		pr_info("ktz8863a write data fail !!\n");

	return ret;
}
EXPORT_SYMBOL(ktz8863a_reg_write_bytes);

int ktz8863a_reg_read_bytes(char addr, char *buf)
{
	int ret = -EINVAL;
	char puReadCmd[1] = {addr};

	if (g_ktz8863a_led.client == NULL && buf == NULL) {
		pr_info("ERROR!! ktz8863a i2c client or buffer is null\n");
		return ret;
	}

	if (addr < KTZ8863A_DISP_REV || addr > KTZ8863A_DISP_PTD_MSB) {
		pr_info("ERROR!! ktz8863a addr overflow\n");
		return ret;
	}

	ret = i2c_master_send(g_ktz8863a_led.client, puReadCmd, 1);
	if (ret < 0) {
		pr_info(" ktz8863a write failed!!\n");
		return ret;
	}

	ret = i2c_master_recv(g_ktz8863a_led.client, buf, 1);
	if (ret < 0) {
		pr_info(" ktz8863a read failed!!\n");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL(ktz8863a_reg_read_bytes);

int ktz8863a_bl_bias_conf(void)
{
	int ret, i, reg_count;

	pr_info("ktz8863a_bl_bias_conf backlight and bias setting\n");
	mutex_lock(&g_ktz8863a_led.lock);

	reg_count = ARRAY_SIZE(ktz8863a_regs_conf) / sizeof(ktz8863a_regs_conf[0]);
	for (i = 0; i < reg_count; i++)
		ret = ktz8863a_reg_write_bytes(ktz8863a_regs_conf[i].reg, ktz8863a_regs_conf[i].value);

	mutex_unlock(&g_ktz8863a_led.lock);
	return ret;
}
EXPORT_SYMBOL(ktz8863a_bl_bias_conf);

int ktz8863a_bias_enable(int enable, int delayMs)
{
	mutex_lock(&g_ktz8863a_led.lock);

	if (delayMs > 100)
		delayMs = 100;

	if (enable) {
		/* enable LCD bias VPOS */
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BIAS_CONF1, 0x9c);
		mdelay(delayMs);
		/* enable LCD bias VNEG */
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BIAS_CONF1, 0x9e);
	} else {
		/* disable LCD bias VNEG */
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BIAS_CONF1, 0x9c);
		mdelay(delayMs);
		/* disable LCD bias VPOS */
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BIAS_CONF1, 0x98);
		mdelay(6);
		/* bias supply off */
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BIAS_CONF1, 0x18);
		mdelay(6);
	}

	mutex_unlock(&g_ktz8863a_led.lock);
	return 0;
}
EXPORT_SYMBOL(ktz8863a_bias_enable);

int ktz8863a_brightness_set(int level)
{
	int tmp_bl = 0;

	if (level < 0 || level > BL_LEVEL_MAX || level == g_ktz8863a_led.level)
		return 0;

	tmp_bl = bl_level_remap[level];
	mutex_lock(&g_ktz8863a_led.lock);

	ktz8863a_reg_write_bytes(KTZ8863A_DISP_BB_LSB, tmp_bl & 0x7);
	ktz8863a_reg_write_bytes(KTZ8863A_DISP_BB_MSB, tmp_bl >> 3);
	pr_info("%s level = %d, tmp_bl = %d", __func__, level, tmp_bl);

	if (level == 0 && g_ktz8863a_led.level != 0) {
		/* disable BL and current sink*/
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BL_ENABLE, 0x0);
		g_ktz8863a_led.hbm_on = 0;
		pr_info("ktz8863a_brightness_set, close\n");
	} else if (level > 0 && g_ktz8863a_led.level == 0) {
		/* enable BL and current sink*/
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BL_ENABLE, 0x17);
		ktz8863a_reg_write_bytes(KTZ8863A_DISP_BC2, 0xcd);
		pr_info("ktz8863a_brightness_set, enable level:%d\n", level);
	}

	g_ktz8863a_led.level = level;
	mutex_unlock(&g_ktz8863a_led.lock);
	return 0;
}
EXPORT_SYMBOL(ktz8863a_brightness_set);

static int ktz8863a_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	pr_info("ktz8863a_probe: %s\n", client->name);
	g_ktz8863a_led.client = client;
	mutex_init(&g_ktz8863a_led.lock);
	g_ktz8863a_led.hbm_on = 0;

	return 0;
}

static int ktz8863a_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);

	return 0;
}

static const struct i2c_device_id ktz8863a_id[] = {
	{ "MI_I2C_LCD_BIAS", 1 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ktz8863a_id);

static const struct of_device_id of_ktz8863a_i2c_match[] = {
	{ .compatible = "mediatek,MI_I2C_LCD_BIAS", },
	{},
};
MODULE_DEVICE_TABLE(of, of_ktz8863a_i2c_match);

static struct i2c_driver ktz8863a_i2c_driver = {
	.driver = {
		.name	= "MI_I2C_LCD_BIAS",
		.of_match_table = of_match_ptr(of_ktz8863a_i2c_match),
	},
	.probe		= ktz8863a_probe,
	.remove		= ktz8863a_remove,
	.id_table	= ktz8863a_id,
};
module_i2c_driver(ktz8863a_i2c_driver);

MODULE_AUTHOR("Lang Lei <leilang1@xiaomi.com>");
MODULE_DESCRIPTION("ktz8863a i2c driver");
MODULE_LICENSE("GPL v2");
