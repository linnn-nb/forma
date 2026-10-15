# 保存、另存为与旧版备份

普通Save已接通；分批9套562项通过，当前资格不等于完整产品或实体文件选择器/听评验收。

1. 未命名工程按CmdS会选择新.tracktionedit路径；已有工程或成功另存后，CmdS/工具栏「保存」写回当前路径。存在参数手势、录音或设备准备时先结束操作；当前GUI播放中保存未放开。
2. CmdShiftS或文件菜单「另存为…」选择新名字，成功后以后CmdS只写新路径。当前已有目标会拒绝覆盖，不能把OS选择器的同名选择当作已实现覆盖式另存。原Save Copy API仍只保存副本、不切换当前路径；退出卡片/恢复副本沿用此语义。
3. 文件被外部修改、删除/替换为链接、锁/备份/写入失败会报错，保留当前Edit与原历史。冲突时重新打开确认外部版本，或另存新名字；不会静默覆盖。打开、另存和保存都只在成功后记录实际文件版本，新建/恢复清除旧绑定。
4. 每次写回前，旧文件独立写入同目录`.forma-backups`，按旧SHA和UUID命名，不被后续自动保存覆盖。用系统「显示隐藏文件」或文件选择器「前往文件夹」访问该目录，按修改时间选择旧版，再作为工程打开或另存新名字；原Undo/Redo在旧版中保留。备份是普通用户文件，不宣称不可人为修改；暂不自动清理/设保留数量，请自行归档。
5. 保存本身不增加工程Undo步骤。CmdZ/Redo只改内存工程，磁盘保持上次保存；再次CmdS才发布新版本。源音频文件不覆盖、不搬运，跨目录另存继续引用它们；第三方插件私有外部素材的可迁移性未验。

L1公开的本地projectFile查询给出当前path和expected_sha256，saveCurrent成功给出新文件SHA、版本和旧版backup路径/SHA。相同完整图像返回unchanged，不改文件也不堆重复旧版。发布后同步/校验若失效，不确认成功，目标可能已变；旧版保留，须核对/重开/另存，不能把错误误解为完全回滚。

实现：Commands::{open,save,saveAs,saveCurrent}/ProjectFileStore、Workspace保存入口与575快捷键；te::Edit仍为唯一工程，外部文件上下文不持久化成另一份工程。合作锁只约束同规范路径的合作保存者；最终哈希检查不是操作系统内容CAS，不承诺拦住忽略锁的任意外部写入。Windows发布分支未实测。

当前证据：ProjectSaveTests64项（实际WAV/原历史/旧版SHA/跨目录音频/真实OS写入故障/第二文件描述符锁/原生Save键与自定义键）、ProductionChain98项含实际CmdS写回与二次导出PCM相同，另有Open/Exit/New/Recovery/Legacy/ProjectDocument/PersistentHistory回归。旧Open轮询超时现场终态缺失；现用真实作业启动通知发Escape并核验晚到写入不切换，原时限保留。索引`build-v2-tracktion/project-save-current-results.json`，详细红绿/构建与未验分支见VERIFICATION.md。

可演示自有测试音工程：`build-v2-tracktion/ProjectSave-ad308f5bcaf84b2aa0ee84e8b9b2c86b/core/Current.tracktionedit`，以及`build-v2-tracktion/P1-Production-Chain-8a94ce4390ab454387daf492651c36e6/P1-Production-Chain.tracktionedit`。保存现有工程并重启唯一App后使用；不是音乐作品。本轮未重启用户正在运行的工程、无新安装包。
