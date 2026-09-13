# AccountAndLicense Module

A self-contained Qt6 CMake static library that handles **serial-ID management**
and **remote licence verification** for any `ktechpit` application.

## Features

- Serial / account-ID generation with obfuscated file persistence
- XOR-encrypted, machine-bound QSettings storage for sensitive licence data
- Binary backup store alongside QSettings (tamper resistance)
- Asynchronous remote licence-status check against a configurable API endpoint
- Local anomaly detection (clock skew, account-ID drift)
- Evaluation-period tracking
- Weekly / lifetime re-check scheduling

## Dependencies

| Dependency   | Version |
|--------------|---------|
| Qt6::Core    | ≥ 6.2   |
| Qt6::Network | ≥ 6.2   |

No other dependencies.  The module is self-contained and can be dropped into
any Qt6 project.

---

## Integration

### 1 – Add the subdirectory

In your project's `CMakeLists.txt`, add the module **before** any target that
uses it:

```cmake
add_subdirectory(path/to/modules/AccountAndLicense)
```

### 2 – Link your target

```cmake
target_link_libraries(my_target PRIVATE account_license)
```

### 3 – Configure and instantiate

```cpp
#include "AccountLicenseConfig.h"
#include "AccountLicenseManager.h"

AccountLicenseConfig cfg;
cfg.appCode             = QStringLiteral("MYAPP");
cfg.appName             = QStringLiteral("My Application");
cfg.settingsOrgName     = QStringLiteral("MyOrg");
cfg.settingsAppName     = QStringLiteral("MyApp");
cfg.checkStatusEndpoint = QStringLiteral("https://example.com/api/check");
cfg.checkoutUrlTemplate = QStringLiteral("https://example.com/checkout?id=%1");
// ...set other endpoints and policy values as needed...

auto *mgr = new AccountLicenseManager(cfg, this);
connect(mgr, &AccountLicenseManager::serialReady,
        this, &MyWindow::onSerialReady);
connect(mgr, &AccountLicenseManager::licenseStatusUpdated,
        this, &MyWindow::onLicenseStatus);

mgr->ensureSerial();
mgr->checkPurchase(mgr->serial());
```

---

## Migrating an existing WonderWall installation

To keep existing user data (activation state, evaluation start date, etc.)
readable after migration, set these backward-compat values in the config:

```cpp
cfg.encryptionKeySuffix = QStringLiteral("WW_LOCAL_LICENSE_KEY_v1");
cfg.keyAliasPrefix      = QStringLiteral("WW_KEY_ALIAS_v1|");
cfg.activationKey       = QStringLiteral("wonderwall");
cfg.serialKey           = QStringLiteral("conf");
```

These match the hard-coded strings that were in `AppSettings.cpp` and
`LicensingConfig.cpp` prior to extraction.

---

## Config reference

| Field                      | Default                         | Notes                                    |
|----------------------------|---------------------------------|------------------------------------------|
| `appCode`                  | _required_                      | Short API code, e.g. `"WW_NG"`           |
| `appName`                  | _required_                      | Human-readable, used in upgrade URLs     |
| `serialPlatformPrefix`     | _(auto)_                        | Prepended to new serial IDs. Empty string = suppress. Null (default) = auto-insert `"win"` on Windows, nothing elsewhere |
| `settingsOrgName`          | _required_                      | QSettings organisation name              |
| `settingsAppName`          | _required_                      | QSettings application name               |
| `encryptionKeySuffix`      | `"AL_LOCAL_LICENSE_KEY_v1"`     | Override for WW compat                   |
| `keyAliasPrefix`           | `"AL_KEY_ALIAS_v1|"`            | Override for WW compat                   |
| `activationKey`            | `"activated"`                   | Override (`"wonderwall"`) for WW compat  |
| `serialKey`                | `"serial"`                      | Override (`"conf"`) for WW compat        |
| `checkStatusEndpoint`      | _required_                      | Full URL to the licence check API        |
| `checkoutUrlTemplate`      | optional                        | `%1` → accountId                         |
| `selfServicePortalUrl`     | optional                        |                                          |
| `upgradeDialogUrlTemplate` | optional                        | `%1` → appName, `%2` → accountId         |
| `evaluationDurationSecs`   | 14 days                         |                                          |
| `weeklyCheckSecs`          | 7 days                          |                                          |
| `lifetimeRecheckSecs`      | 90 days                         |                                          |
| `clockSkewToleranceSecs`   | 10 minutes                      |                                          |
| `serialStoreDirLeaf`       | `"._t6p2h"`                     | Obfuscated dir leaf under AppDataLocation |
| `serialStoreFileName`      | `"._i3n.dat"`                   |                                          |
| `backupStoreDirLeaf`       | `"._a9v1k"`                     |                                          |
| `backupStoreFileName`      | `"._s4d.dat"`                   |                                          |

