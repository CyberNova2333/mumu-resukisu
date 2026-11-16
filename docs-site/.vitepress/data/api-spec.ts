import type { ApiCatalogSpec } from './api-types';

export const apiSpecZh: ApiCatalogSpec = {
  project: 'MuMu ReSukiSU · 提取、构建、VDI 审计与单实例事务参考',
  subtitle:
    '覆盖固定指纹目标 MuMu Android 15 x86_64 的 10 个 mumu-resukisu 主包子命令、8 组离线构建/准备脚本及 profile/userspace JSON 契约。',
  environments: [
    { label: '目标档案 (MuMu Android 15 x86_64)', value: 'targets/mumu-android15-x86_64/profile.json' },
    { label: '本机用户态绑定示例', value: 'bindings/userspace.json' },
  ],
  searchPlaceholder: '搜索 extract、assemble、bind、install、restore、build-native…',
  emptyText: '没有找到匹配的 CLI 子命令、构建脚本或 JSON 契约。',
  labels: {
    allGroups: '全部',
    canDo: '✅ 能做到',
    cannotDo: '⛔ 明确限制 / 拒绝条件',
    params: '命令参数、档案字段与硬门槛',
    paramName: '参数 / 字段',
    paramLocation: '来源',
    paramType: '类型 / 约束',
    paramRequired: '要求',
    paramDesc: '校验语义与失败行为',
    responses: '输出产物与状态语义',
    examples: '标准调用示例',
    responseSample: '输出 / 清单示例',
    copy: '复制',
    copied: '已复制',
    noParams: '无额外参数。',
    required: '必填',
    optional: '可选',
    deepDocs: '查看输入契约与 CLI 全表',
    totalItems: '组命令/脚本',
    envLabel: '档案路径基准',
    mobilePicker: '切换命令或脚本',
    defaultLabel: '默认',
    plainIndexToggle: '纯文本检索与打印索引（Ctrl+K 站内搜索定位）',
  },
  groups: [
    { id: 'offline-build', title: '离线提取、原生编译与分区准备' },
    { id: 'vdi-windows', title: '整盘 VDI 审计与 Windows 事务安装/恢复' },
    { id: 'release-channel', title: '上游发行版查询与待审登记' },
  ],
  items: [
    {
      id: 'profile-and-extract',
      group: 'offline-build',
      kind: 'CLI',
      title: '目标档案核对与原始分区提取 (profile / extract)',
      signature: 'mumu-resukisu profile|extract --profile {{BASE_URL}} ...',
      auth: '本地只读快照 + SHA256 指纹核验',
      stability: '跨平台只读入口',
      summary:
        '验证 targets/mumu-android15-x86_64/profile.json 的 schemaVersion(1)、androidVersion("15.0")、baseSha256、virtualBytes(2,465,270,784) 与三分区偏移摘要，从只读 VDI 链提取 boot/system/vendor 镜像。',
      relatedLink: '/docs/configuration',
      relatedLabel: '查看 profile.json 约束',
      capabilities: [
        'extract 在读取前及写完后二次核对 base 与 source 摘要（防止提取期间底层快照被改动），写出 boot.img、system.img、vendor.img 与 extraction.json',
        'profile 无需触碰真实模拟器磁盘即可解析并输出允许修改的分区范围及约束',
      ],
      limitations: [
        '绝对不支持 Android 12 或其他未指纹化 MuMu 引擎；禁止通过手改 profile.json 期望摘要强行放行未知磁盘',
        'output 目录必须尚不存在（mkdir parents=True, exist_ok=False），若因容量或摘要不符中止则保留失败现场供排查，重试必须更换输出目录',
        '仓库不分发任何原盘 base.vdi/source.vdi，须由使用者在受忽略 inputs/ 自行准备',
      ],
      params: [
        {
          name: '--profile',
          location: 'flag',
          type: 'Path (profile.json)',
          required: true,
          description: '目标档案 JSON；校验 id、64 位十六进制 SHA256 与 virtualBytes (2465270784)。',
        },
        {
          name: '--base / --source / --output',
          location: 'flag',
          type: 'Path (extract 必填)',
          required: true,
          description: '共享基础盘、差分系统盘副本及尚不存在的输出目录。',
        },
      ],
      responses: [
        {
          status: 'JSON extraction.json (exit 0)',
          tone: 'ok',
          summary: '记录 profileId、sourceSha256 与提取后三分区 sha256。',
        },
        {
          status: 'ValueError (非零退出)',
          tone: 'err',
          summary: 'Wrong base fingerprint / Wrong virtual capacity / Unknown source partition / Input changed。',
        },
      ],
      examples: [
        {
          label: '检查目标档案并提取三分区镜像',
          lang: 'sh',
          code: `mumu-resukisu profile --profile "{{BASE_URL}}"\nmumu-resukisu extract --profile "{{BASE_URL}}" --base inputs/base.vdi --source inputs/source.vdi --output build/extracted`,
        },
      ],
    },
    {
      id: 'build-native-and-kernel',
      group: 'offline-build',
      kind: 'CLI',
      title: '原生内核模块与重打包内核构建 (build-native.py / prepare-kernel.py)',
      signature: 'python3 scripts/build-native.py --resukisu-source inputs/resukisu ...',
      auth: 'Linux / WSL 构建工具链 + target-profile.json',
      stability: 'Linux / WSL 离线构建',
      summary:
        '按 native/driver/target-profile.json 导出固定上游 Git 提交源码，编译 kernelsu.ko（vermagic=6.1.90-perf+ 、versionCode=35253、UAPI v6、moduleStructBytes=960）与 driver-init 加载器，并验证精确 235 个未定义外部符号。',
      relatedLink: '/docs/',
      relatedLabel: '查看构建环境依赖与步骤',
      capabilities: [
        '从 --resukisu-source 导出锁定提交的干净源码（不带入未提交本地改动），运行加载器主机断言（非 PID 1 退出 64）',
        'prepare-kernel.py 对原内核与 ELF 执行双解压交叉核对、原压缩槽尺寸与 CRC 检查',
      ],
      limitations: [
        '仅支持 Linux 或 WSL（依赖 Clang/LLVM、make、musl-gcc、gcc、pyelftools、Unicorn）',
        '不允许直接传入 ZIP/TAR 源码归档作为 --resukisu-source（必须为真实 Git 工作树）',
      ],
      params: [
        {
          name: '--resukisu-source / --kernel-archive / --kernel-config',
          location: 'flag',
          type: 'Path (build-native)',
          required: true,
          description: '上游 Git 检出、ACK 内核归档与目标内核配置。',
        },
        {
          name: '--clang / --target-symbols / --output',
          location: 'flag',
          type: 'Path',
          required: true,
          description: 'Clang 工具链、core-kallsyms.txt（核对 235 个外部符号）及输出目录。',
        },
      ],
      responses: [
        {
          status: 'build/native/manifest.json + kernelsu.ko + driver-init',
          tone: 'ok',
          summary: '包含全量 235 符号校验通过记录与加载器自测结果。',
        },
      ],
      examples: [
        {
          label: '编译原生驱动并重打包内核',
          lang: 'sh',
          code: `python3 scripts/build-native.py \\\n  --resukisu-source inputs/resukisu --kernel-archive inputs/ack.tar.gz \\\n  --kernel-config inputs/prepared.config --clang inputs/clang \\\n  --target-symbols inputs/core-kallsyms.txt --output build/native\npython3 scripts/prepare-kernel.py --kernel inputs/kernel --elf inputs/kernel.elf --output build/kernel`,
        },
      ],
    },
    {
      id: 'build-userspace-and-partitions',
      group: 'offline-build',
      kind: 'CLI',
      title: '用户态预置工具与 boot/system/vendor 分区构建',
      signature: 'python3 scripts/build-userspace.py --config bindings/userspace.json ... & prepare-*.py',
      auth: '三键 userspace.json + supported-daemons.json + stock-daemon.json',
      stability: '离线分区制备',
      summary:
        '核对固定 64 位 ksud（6,001,344 字节）与原厂 /system/xbin/dusk（3,753,880 字节）指纹，构建 resukisu-migrate-prestage 并执行 ASAN/UBSAN 失败注入与 32 位主机模型；随后按数据槽与 e2fsck 边界更新三大分区。',
      relatedLink: '/docs/configuration',
      relatedLabel: '查看 userspace.json 与辅助脚本',
      capabilities: [
        'bindings/userspace.json 严格限制仅 3 键：schemaVersion (1)、expectedManagerUid (10000..99999) 与 daemonFile',
        'prepare-boot.py 保持原槽尺寸写入加载器与预置程序；prepare-system.py 使用 e2fsprogs/libext2fs 编辑守护进程并过 e2fsck；prepare-vendor.py 仅替换启动覆盖条目',
        '可选 scripts/build-frida.py 单独构建 116,223,160 字节 Frida 17.19.0 辅助组件（不接入主安装流）',
      ],
      limitations: [
        '用户态产物清单明确标记 deviceValidated: false（仅通过主机 ASAN/UBSAN 与模型测试，未含目标冷启动验收）',
        '修改后保留 AVB 尾部但签名校验树已不匹配原签名，严禁将候选盘误当原厂已验签镜像使用',
        '不接受 32 位 daemon（supported-daemons.json 仅认固定 64 位 ksud 摘要），禁止跨实例照抄 expectedManagerUid',
      ],
      params: [
        {
          name: 'schemaVersion / expectedManagerUid / daemonFile',
          location: 'config',
          type: '1 / int(10000..99999) / Path',
          required: true,
          description: 'userspace.json 三键契约，缺项或多余字段立即报错。',
        },
        {
          name: '--stock-daemon (build-userspace)',
          location: 'flag',
          type: 'Path (3,753,880 bytes)',
          required: true,
          description: '从原系统提取的 /system/xbin/dusk 样本，不符 stock-daemon.json 则拒绝构建。',
        },
      ],
      responses: [
        {
          status: 'build/userspace/manifest.json + *.img',
          tone: 'warn',
          summary: '产出分区候选镜像及显式标明 deviceValidated: false 的清单。',
          example: `{ "schemaVersion": 1, "deviceValidated": false, "stockDaemonBytes": 3753880 }`,
        },
      ],
      examples: [
        {
          label: '构建预置程序与 boot/system/vendor 分区',
          lang: 'sh',
          code: `python3 scripts/build-userspace.py --config bindings/userspace.json --stock-daemon inputs/stock-dusk --output build/userspace\npython3 scripts/prepare-boot.py --source build/extracted/boot.img --kernel build/kernel/kernel --loader build/native/driver-init --module build/native/kernelsu.ko --native-manifest build/native/manifest.json --userspace-manifest build/userspace/manifest.json --prestage build/userspace/resukisu-migrate-prestage --output build/boot`,
        },
      ],
    },
    {
      id: 'assemble-and-audit',
      group: 'vdi-windows',
      kind: 'CLI',
      title: '差分盘合成与全盘 2.46 GB COW 字节审计 (assemble / audit)',
      signature: 'mumu-resukisu assemble|audit --profile {{BASE_URL}} ...',
      auth: '本地 VDI 块语义全盘校验',
      stability: '跨平台离线审计',
      summary:
        '将修改后的 boot/system/vendor 写回候选差分 VDI，并按父盘、差分盘、零块与未分配块 1 MiB VDI COW 语义逐字节审计全部 2,465,270,784 逻辑字节。',
      relatedLink: '/docs/image-boundaries',
      relatedLabel: '查看镜像变化与审计边界',
      capabilities: [
        '生成同目录基名关联清单 build/candidate/candidate.json（diskFile / diskSha256 / auditFile / auditSha256）',
        'audit 支持在写入前或任意时刻独立重算全部虚拟字节，保证三分区以外零越界改写',
      ],
      limitations: [
        'assemble 绝不触碰或重启模拟器实例，也不修改 inputs/base.vdi 与 source.vdi；清单内 artifact 必须为相邻文件名，禁止跨目录路径',
      ],
      params: [
        {
          name: '--partition NAME=FILE',
          location: 'flag',
          type: 'boot=... system=... vendor=...',
          required: true,
          description: 'assemble 必填且不可重复的分区映射参数。',
        },
        {
          name: '--candidate (audit)',
          location: 'flag',
          type: 'Path (candidate disk / json)',
          required: true,
          description: '待重算审计的候选磁盘文件路径。',
        },
      ],
      responses: [
        {
          status: 'candidate.json + audit.json (exit 0)',
          tone: 'ok',
          summary: '确认 sourceDiskSha256、candidateSha256 与允许分区外零越界。',
        },
      ],
      examples: [
        {
          label: '重组候选差分 VDI 并复查审计结果',
          lang: 'powershell',
          code: `mumu-resukisu assemble --profile "{{BASE_URL}}" --base inputs/base.vdi --source inputs/source.vdi \\\n  --partition boot=build/boot/boot.img --partition system=build/system/system-candidate.img --partition vendor=build/vendor/vendor.img \\\n  --output build/candidate`,
        },
      ],
    },
    {
      id: 'bind-inspect-install',
      group: 'vdi-windows',
      kind: 'CLI',
      title: '单实例绑定、预检与 Windows 独占锁事务写入 (bind / inspect / install)',
      signature: 'mumu-resukisu bind|inspect|install --profile {{BASE_URL}} ...',
      auth: 'MuMuManager.exe 实时查询 + Win32 CreateFileW (share=0)',
      stability: 'Windows 事务写盘',
      summary:
        '显式绑定单一已停机测试实例并执行非写入干跑预检；install 经连续三次停机轮询与内置 Root 关闭校验后取得 Windows 独占写锁，先完整持久化 rollback.vdi 再写盘并回读校验。',
      relatedLink: '/docs/walkthrough',
      relatedLabel: '查看一次可回退安装全流程',
      capabilities: [
        'bind 调用 MuMuManager.exe info -v <index> 核对 index/name/created_timestamp/android_version ("15.0")，拒绝自动猜测或批量绑定',
        'install 通过 Win32 CreateFileW (share=0) + msvcrt.locking 独占锁定私有差分盘，写完候选差分字节后强制回读核验 SHA256',
      ],
      limitations: [
        '写盘锁依赖 Windows API（Linux unittest 会跳过相关 29 项用例）；install 写完后刻意不自动启动模拟器',
        '当前 refactoredDeviceValidation 仍为 false（重整后冷启动实机验证还在分阶段推进中），绑定文件 bindings/test.json 禁止与多套 .state 副本混用',
      ],
      params: [
        {
          name: '--manager / --index / --instance-directory / --private-disk',
          location: 'flag',
          type: 'Path / string (bind)',
          required: true,
          description: '显式传入 MuMuManager.exe 路径、实例编号、实例目录与私有差分盘路径（绝不自动扫描）。',
        },
        {
          name: '--binding / --candidate',
          location: 'flag',
          type: 'Path (inspect/install)',
          required: true,
          description: '已绑定 JSON 档案（旁侧保存 .state/<sha256>/ 事务日志）及同级目录 candidate.json。',
        },
      ],
      responses: [
        {
          status: 'bind -> deviceWritten: false / install -> committed transaction',
          tone: 'ok',
          summary: 'inspect 不写盘只校验 live 基准；install 写入私有差分盘并保留 rollback.vdi。',
        },
      ],
      examples: [
        {
          label: '绑定实例、干跑预检并写入测试实例',
          lang: 'powershell',
          code: `mumu-resukisu inspect --profile "{{BASE_URL}}" --binding bindings/test.json --candidate build/candidate/candidate.json\nmumu-resukisu install --profile "{{BASE_URL}}" --binding bindings/test.json --candidate build/candidate/candidate.json`,
        },
      ],
    },
    {
      id: 'status-and-restore',
      group: 'vdi-windows',
      kind: 'CLI',
      title: '事务状态查询与原盘原子恢复 (status / restore)',
      signature: 'mumu-resukisu status|restore --profile {{BASE_URL}} --binding bindings/test.json',
      auth: '.state/<sha256>/ 日志 + rollback.vdi 校验',
      stability: '可逆回退入口',
      summary:
        '查询绑定实例旁侧 .state/<sha256>/ 中的最新事务编号与备份摘要；restore 验证停机状态并持有独占锁后，把私有系统盘逐字节还原为安装前原貌。',
      relatedLink: '/docs/image-boundaries',
      relatedLabel: '查看恢复范围边界',
      capabilities: [
        'restore 仅依赖 .state/<sha256>/ 中的事务日志与 rollback.vdi，即使 build/candidate 已被清理仍可完整回退',
        '保持绑定文件与 .state 目录相邻时支持幂等查询最新事务 ID 与备份校验结果',
      ],
      limitations: [
        'restore 只还原 Windows 主机侧私有系统差分盘（system-diff.vdi），刻意不改写或清空 Android /data 目录中的管理器记录与原厂备份',
      ],
      params: [
        {
          name: '--binding',
          location: 'flag',
          type: 'Path (bindings/test.json)',
          required: true,
          description: '必须与同一 .state/<sha256>/ 目录配对保留。',
        },
        {
          name: '--transaction (restore)',
          location: 'flag',
          type: 'string (Tx ID)',
          required: true,
          description: '由 status 返回的目标事务编号。',
        },
      ],
      responses: [
        {
          status: 'Restored / Verified',
          tone: 'ok',
          summary: '私有系统差分盘恢复至事务前 SHA256。',
        },
      ],
      examples: [
        {
          label: '查询最近事务并按事务号回退',
          lang: 'powershell',
          code: `mumu-resukisu status --profile "{{BASE_URL}}" --binding bindings/test.json\nmumu-resukisu restore --profile "{{BASE_URL}}" --binding bindings/test.json --transaction "<TxId>"`,
        },
      ],
    },
    {
      id: 'fetch-and-register-release',
      group: 'release-channel',
      kind: 'CLI',
      title: '上游版本独立查询下载与待审登记 (fetch-release / register-release.py)',
      signature: 'mumu-resukisu fetch-release [--channel prerelease] [--check] [--output ...]',
      auth: 'HTTPS 公共发行 API（不改写固定目标）',
      stability: '隔离辅助通道',
      summary:
        '面向已登记官方仓库身份查询或下载 ReSukiSU / BakaSU 正式版与预发行资产；register-release.py 生成隔离的待审 entry.json 与 proof.json 供人工审查。',
      relatedLink: '/docs/release-lifecycle',
      relatedLabel: '查看发行版兼容性边界',
      capabilities: [
        '仅面向硬编码核验通过的官方上游仓库身份，区分 stable 与 prerelease 通道',
        'register-release.py 同时校验 APK 签名、UAPI 与源码包身份，输出附带 catalogUpdated: false 的审计证据',
      ],
      limitations: [
        '默认只查 stable，若上游无正式版则直接报错不静默降级至 prerelease；命令本身从不安装任何镜像',
        '下载的源码压缩包仅作调查用途，禁止直接传给 build-native.py --resukisu-source，且不会自动更新 native/driver/target-profile.json 或 supported-daemons.json',
      ],
      params: [
        {
          name: '--channel',
          location: 'flag',
          type: 'stable (default) | prerelease',
          required: false,
          description: '默认仅查询正式版，无正式版时报错退出 1。',
        },
        {
          name: '--check / --output',
          location: 'flag',
          type: 'bool / Path',
          required: false,
          description: '--check 仅核对元数据；--output 将资产下载到独立目录（如 inputs/latest-release）。',
        },
      ],
      responses: [
        {
          status: 'JSON release metadata / HTTP Error (exit 1)',
          tone: 'warn',
          summary: '返回发行版本号与资产清单，或明确报告 HTTP/速率限制错误。',
        },
      ],
      examples: [
        {
          label: '核对上游预发行版并下载至隔离目录',
          lang: 'powershell',
          code: `mumu-resukisu fetch-release --channel prerelease --check\nmumu-resukisu fetch-release --channel prerelease --output inputs/latest-release`,
        },
      ],
    },
  ],
};

export const apiSpecEn: ApiCatalogSpec = {
  project: 'MuMu ReSukiSU · Build, VDI Audit & Reversible Install CLI Reference',
  subtitle:
    'Complete command, helper script, and JSON schema reference for the fingerprinted MuMu Android 15 x86_64 root pipeline: 10 mumu-resukisu subcommands, 8 Linux/WSL build helpers, and Win32 transactional disk contracts.',
  environments: [
    { label: 'Target Fingerprint Profile', value: 'targets/mumu-android15-x86_64/profile.json' },
    { label: 'Userspace Binding Manifest', value: 'bindings/userspace.json' },
  ],
  searchPlaceholder: 'Search extract, assemble, bind, install, restore, build-native…',
  emptyText: 'No matching CLI subcommand, helper script, or schema found.',
  labels: {
    allGroups: 'All',
    canDo: '✅ Supported Capabilities',
    cannotDo: '⛔ Explicit Limits / Rejection Rules',
    params: 'CLI Flags, Schema Keys & Hard Gates',
    paramName: 'Flag / Key',
    paramLocation: 'Scope',
    paramType: 'Type / Constraint',
    paramRequired: 'Required',
    paramDesc: 'Verification & Rejection Behavior',
    responses: 'Outputs & Status Semantics',
    examples: 'Standard Usage Examples',
    responseSample: 'Sample Output / Manifest',
    copy: 'Copy',
    copied: 'Copied',
    noParams: 'No additional parameters required.',
    required: 'Required',
    optional: 'Optional',
    deepDocs: 'Open Full Contracts & CLI Matrix',
    totalItems: 'commands documented',
    envLabel: 'Profile Base',
    mobilePicker: 'Select command or script',
    defaultLabel: 'Default',
    plainIndexToggle: 'Plain-Text Index & Search Anchors (Ctrl+K searchable)',
  },
  groups: [
    { id: 'offline-build', title: 'Extraction, Native Build & Partitions' },
    { id: 'vdi-windows', title: 'Whole-Disk Audit & Windows Transactions' },
    { id: 'release-channel', title: 'Release Lookup & Isolated Staging' },
  ],
  items: [
    {
      id: 'profile-and-extract',
      group: 'offline-build',
      kind: 'CLI',
      title: 'Target Fingerprint Verification & Partition Extract (profile / extract)',
      signature: 'mumu-resukisu profile|extract --profile {{BASE_URL}} ...',
      auth: 'Read-only local snapshots + SHA256 verification',
      stability: 'Cross-Platform Read-Only',
      summary:
        'Validates schemaVersion(1), androidVersion("15.0"), baseSha256, 2,465,270,784 virtualBytes, and boot/system/vendor source hashes before extracting partitions to a fresh output directory.',
      relatedLink: '/en/docs/configuration',
      relatedLabel: 'See profile.json & CLI Matrix',
      capabilities: [
        'extract hashes base and source snapshots before and after reading and writes extraction.json + partition images',
        'profile parses and validates fingerprint gates without touching emulator disks',
      ],
      limitations: [
        'Rejects Android 12 or any non-matching MuMu disk engine; never relax expected SHA256 digests manually',
        'Fails if --output already exists (exist_ok=False) and retains failed snapshots until retried in a new directory',
        'Repository ships zero disk images, kernels, or APKs; provide them locally under gitignored inputs/',
      ],
      params: [
        {
          name: '--profile',
          location: 'flag',
          type: 'Path (profile.json)',
          required: true,
          description: 'Enforces exact virtualBytes (2465270784) and 64-char hex partition SHA256s.',
        },
        {
          name: '--base / --source / --output',
          location: 'flag',
          type: 'Path (extract)',
          required: true,
          description: 'Stopped snapshot base/source VDIs and non-existent target output folder.',
        },
      ],
      responses: [
        {
          status: 'extraction.json (exit 0) / ValueError',
          tone: 'ok',
          summary: 'Outputs extracted hashes or aborts cleanly on Wrong base/virtual/partition fingerprint.',
        },
      ],
      examples: [
        {
          label: 'Validate target profile and extract source partitions',
          lang: 'sh',
          code: `mumu-resukisu profile --profile "{{BASE_URL}}"\nmumu-resukisu extract --profile "{{BASE_URL}}" --base inputs/base.vdi --source inputs/source.vdi --output build/extracted`,
        },
      ],
    },
    {
      id: 'build-native-and-kernel',
      group: 'offline-build',
      kind: 'CLI',
      title: 'Native Kernel Module & Kernel Prep (build-native.py / prepare-kernel.py)',
      signature: 'python3 scripts/build-native.py --resukisu-source inputs/resukisu ...',
      auth: 'Linux / WSL toolchain + target-profile.json',
      stability: 'Linux / WSL Only',
      summary:
        'Exports the pinned upstream Git commit, builds kernelsu.ko (vermagic=6.1.90-perf+ , versionCode=35253, UAPI v6, moduleStructBytes=960) and driver-init, checks all 235 undefined symbols, and validates kernel slot bounds.',
      relatedLink: '/en/docs/',
      relatedLabel: 'See Native Build Prerequisites',
      capabilities: [
        'Exports clean pinned Git state without dirty local changes and runs loader host assertions (non-PID 1 exits 64)',
        'prepare-kernel.py double-decompresses and checks slot size bounds and CRC',
      ],
      limitations: [
        'Requires Linux or WSL toolchain (Clang/LLVM, make, musl-gcc, pyelftools, Unicorn)',
        'Archive downloads cannot be passed as --resukisu-source (requires a real Git repository)',
      ],
      params: [
        {
          name: '--resukisu-source / --kernel-archive / --kernel-config',
          location: 'flag',
          type: 'Path',
          required: true,
          description: 'Pinned ReSukiSU Git tree, ACK tarball, and prepared kernel config.',
        },
        {
          name: '--clang / --target-symbols / --output',
          location: 'flag',
          type: 'Path',
          required: true,
          description: 'Clang binary dir, core-kallsyms.txt (235 symbol check), and output folder.',
        },
      ],
      responses: [
        {
          status: 'build/native/manifest.json + kernelsu.ko + driver-init',
          tone: 'ok',
          summary: 'Verified module structure (960 bytes) and loader binary.',
        },
      ],
      examples: [
        {
          label: 'Build kernelsu.ko and repack kernel on Linux/WSL',
          lang: 'sh',
          code: `python3 scripts/build-native.py \\\n  --resukisu-source inputs/resukisu --kernel-archive inputs/ack.tar.gz \\\n  --kernel-config inputs/prepared.config --clang inputs/clang \\\n  --target-symbols inputs/core-kallsyms.txt --output build/native\npython3 scripts/prepare-kernel.py --kernel inputs/kernel --elf inputs/kernel.elf --output build/kernel`,
        },
      ],
    },
    {
      id: 'build-userspace-and-partitions',
      group: 'offline-build',
      kind: 'CLI',
      title: 'Userspace Prestage & Boot/System/Vendor Preparation',
      signature: 'python3 scripts/build-userspace.py --config bindings/userspace.json ... & prepare-*.py',
      auth: '3-key userspace.json + supported-daemons.json + stock-daemon.json',
      stability: 'Offline Partition Prep',
      summary:
        'Verifies fixed 64-bit ksud (6,001,344 bytes) and stock /system/xbin/dusk (3,753,880 bytes), builds resukisu-migrate-prestage with ASAN/UBSAN stock-unit host tests, and updates boot/system/vendor within strict slot/ext4 bounds.',
      relatedLink: '/en/docs/configuration',
      relatedLabel: 'See userspace.json Schema',
      capabilities: [
        'Validates exact 3-key bindings/userspace.json: schemaVersion(1), expectedManagerUid(10000..99999), daemonFile',
        'prepare-system.py updates only target daemon entries via e2fsprogs/libext2fs and validates with e2fsck',
        'Optional scripts/build-frida.py handles standalone 116,223,160-byte Frida 17.19.0 tooling outside the main pipeline',
      ],
      limitations: [
        'Emitted manifest explicitly carries deviceValidated: false until cold-boot tested on target',
        'Retains AVB trailer bytes while invalidating original AVB signature tree (not a generic signed ROM)',
        'Rejects 32-bit daemons and any unknown stock /system/xbin/dusk hash',
      ],
      params: [
        {
          name: 'schemaVersion / expectedManagerUid / daemonFile',
          location: 'config',
          type: '1 / int(10000..99999) / Path',
          required: true,
          description: 'Three required keys in bindings/userspace.json; extra keys are rejected.',
        },
        {
          name: '--stock-daemon',
          location: 'flag',
          type: 'Path (3,753,880 bytes)',
          required: true,
          description: 'Extracted stock /system/xbin/dusk checked against stock-daemon.json.',
        },
      ],
      responses: [
        {
          status: 'build/userspace/manifest.json + *.img',
          tone: 'warn',
          summary: 'Outputs partition images with deviceValidated: false boundary preserved.',
          example: `{ "schemaVersion": 1, "deviceValidated": false, "stockDaemonBytes": 3753880 }`,
        },
      ],
      examples: [
        {
          label: 'Build prestage migration binary and patch boot image',
          lang: 'sh',
          code: `python3 scripts/build-userspace.py --config bindings/userspace.json --stock-daemon inputs/stock-dusk --output build/userspace`,
        },
      ],
    },
    {
      id: 'assemble-and-audit',
      group: 'vdi-windows',
      kind: 'CLI',
      title: 'VDI Candidate Assembly & Whole-Disk COW Byte Audit (assemble / audit)',
      signature: 'mumu-resukisu assemble|audit --profile {{BASE_URL}} ...',
      auth: 'Full 2,465,270,784-byte COW verification',
      stability: 'Cross-Platform Audit',
      summary:
        'Assembles modified boot/system/vendor partitions into a candidate diff VDI and audits every logical byte across parent, diff, zero, and unallocated 1 MiB VDI COW blocks.',
      relatedLink: '/en/docs/image-boundaries',
      relatedLabel: 'See Image & Recovery Boundaries',
      capabilities: [
        'Writes co-located candidate.json + audit.json with exact sourceDiskSha256 and candidateSha256 digests',
        'Proves zero byte modifications outside declared boot/system/vendor offsets',
      ],
      limitations: [
        'Never writes to live emulator disks; manifest artifact paths must be adjacent basenames only',
      ],
      params: [
        {
          name: '--partition NAME=FILE',
          location: 'flag',
          type: 'boot=... system=... vendor=...',
          required: true,
          description: 'Partition replacement images (duplicate names forbidden).',
        },
      ],
      responses: [
        {
          status: 'candidate.json + audit.json',
          tone: 'ok',
          summary: 'Audited whole-disk diff artifact ready for Windows preflight.',
        },
      ],
      examples: [
        {
          label: 'Assemble candidate VDI and run full audit',
          lang: 'powershell',
          code: `mumu-resukisu assemble --profile "{{BASE_URL}}" --base inputs/base.vdi --source inputs/source.vdi --partition boot=build/boot/boot.img --partition system=build/system/system-candidate.img --partition vendor=build/vendor/vendor.img --output build/candidate`,
        },
      ],
    },
    {
      id: 'bind-inspect-install',
      group: 'vdi-windows',
      kind: 'CLI',
      title: 'Single-Instance Bind, Preflight Inspect & Transactional Install (Windows)',
      signature: 'mumu-resukisu bind|inspect|install --profile {{BASE_URL}} ...',
      auth: 'MuMuManager.exe query + Win32 CreateFileW (share=0)',
      stability: 'Windows Write Lock',
      summary:
        'Binds one explicitly selected stopped instance, dry-runs live baseline/candidate checks via inspect, and atomically backs up rollback.vdi under exclusive Win32 locks before flashing private diff bytes and verifying readback.',
      relatedLink: '/en/docs/walkthrough',
      relatedLabel: 'See Reversible Run Walkthrough',
      capabilities: [
        'bind queries MuMuManager.exe info -v <index> for index, name, created_timestamp, and android_version (15.0) — never auto-guesses instances',
        'install polls 3 consecutive shutdown checks + disabled built-in root, takes share=0 + msvcrt.locking, writes rollback.vdi, flashes diff, and re-hashes',
      ],
      limitations: [
        'Exclusive disk locking requires Windows (29 Win32 tests skip cleanly on Linux); install intentionally never boots the VM automatically',
        'profile.json refactoredDeviceValidation remains false while cold-boot staged validation proceeds; do not clone binding files across separate .state dirs',
      ],
      params: [
        {
          name: '--manager / --index / --instance-directory / --private-disk',
          location: 'flag',
          type: 'Path / string',
          required: true,
          description: 'Explicit MuMuManager.exe path, index, instance dir, and system-diff.vdi.',
        },
        {
          name: '--binding / --candidate',
          location: 'flag',
          type: 'Path',
          required: true,
          description: 'Binding file paired with adjacent .state/<sha256>/ journal and candidate.json.',
        },
      ],
      responses: [
        {
          status: 'bind -> deviceWritten: false / install -> committed transaction',
          tone: 'ok',
          summary: 'Writes candidate diff with persistent rollback.vdi and transaction journal.',
        },
      ],
      examples: [
        {
          label: 'Run preflight inspect and exclusive-lock install on Windows',
          lang: 'powershell',
          code: `mumu-resukisu inspect --profile "{{BASE_URL}}" --binding bindings/test.json --candidate build/candidate/candidate.json\nmumu-resukisu install --profile "{{BASE_URL}}" --binding bindings/test.json --candidate build/candidate/candidate.json`,
        },
      ],
    },
    {
      id: 'status-and-restore',
      group: 'vdi-windows',
      kind: 'CLI',
      title: 'Transaction Status & Atomic Disk Rollback (status / restore)',
      signature: 'mumu-resukisu status|restore --profile {{BASE_URL}} --binding bindings/test.json',
      auth: '.state/<sha256>/ journal + rollback.vdi digest check',
      stability: 'Atomic Rollback',
      summary:
        'Reads transaction journals from .state/<sha256>/ beside the binding file and restores the private diff VDI byte-for-byte from rollback.vdi.',
      relatedLink: '/en/docs/image-boundaries',
      relatedLabel: 'See Recovery Boundaries',
      capabilities: [
        'restore succeeds even after build/candidate artifacts have been deleted, provided .state/<sha256>/rollback.vdi remains intact',
      ],
      limitations: [
        'Restores only the private Windows system diff disk (system-diff.vdi) and never mutates or wipes Android /data manager grants or stock backups',
      ],
      params: [
        {
          name: '--transaction (restore)',
          location: 'flag',
          type: 'string',
          required: true,
          description: 'Transaction ID reported by mumu-resukisu status.',
        },
      ],
      responses: [
        {
          status: 'Restored to baseline SHA256',
          tone: 'ok',
          summary: 'Verified rollback completed under shutdown and Win32 exclusive lock.',
        },
      ],
      examples: [
        {
          label: 'Inspect transaction state and restore original disk',
          lang: 'powershell',
          code: `mumu-resukisu status --profile "{{BASE_URL}}" --binding bindings/test.json\nmumu-resukisu restore --profile "{{BASE_URL}}" --binding bindings/test.json --transaction "<TxId>"`,
        },
      ],
    },
    {
      id: 'fetch-and-register-release',
      group: 'release-channel',
      kind: 'CLI',
      title: 'Upstream Release Query & Isolated Review Registration',
      signature: 'mumu-resukisu fetch-release [--channel prerelease] [--check] [--output ...]',
      auth: 'Registered upstream repo identity only',
      stability: 'Isolated Helper',
      summary:
        'Queries or downloads registered official ReSukiSU / BakaSU releases into an isolated staging directory; register-release.py checks APK signatures/UAPI and outputs entry.json + proof.json with catalogUpdated: false.',
      relatedLink: '/docs/release-lifecycle',
      relatedLabel: 'See Release Lifecycle (ZH)',
      capabilities: [
        'Enforces pinned official upstream identity checks and separates stable vs prerelease channels explicitly',
      ],
      limitations: [
        'Default stable channel fails instead of silently falling back to prerelease when no stable release exists',
        'Downloaded archives cannot be passed directly to --resukisu-source and never auto-update target-profile.json or supported-daemons.json',
      ],
      params: [
        {
          name: '--channel / --check / --output',
          location: 'flag',
          type: 'stable|prerelease / bool / Path',
          required: false,
          description: 'Metadata check or isolated download path (never installs).',
        },
      ],
      responses: [
        {
          status: 'Release JSON / HTTP Error (exit 1)',
          tone: 'warn',
          summary: 'Returns version/asset inventory or explicit HTTP rate-limit error.',
        },
      ],
      examples: [
        {
          label: 'Check upstream prerelease metadata',
          lang: 'powershell',
          code: `mumu-resukisu fetch-release --channel prerelease --check`,
        },
      ],
    },
  ],
};
