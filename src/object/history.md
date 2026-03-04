# Object 模块开发日志 (History)

本文档记录 src/object 模块的开发与变更。

---

## [Unreleased]

### Object Manager 功能增强
- **类型注册表持久化**：新增 `object_manager_save_registry` 和 `object_manager_load_registry`
  - 支持将对象类型的 id 和 name 保存到 ODB
  - 加载时验证当前 manager 是否已注册所有需要的类型
  - 支持类型 id 重映射，解决保存时和加载时类型 id 可能不同的问题
- **统一加载接口**：合并 `object_manager_load` 和 `object_manager_load_with_context`
  - `object_manager_load` 现在接受可选的 `id_map` 参数
  - 如果 `id_map` 为 NULL，直接使用读取的 type_id（原行为）
  - 如果 `id_map` 不为 NULL，通过映射表转换 type_id（支持 id 重映射）

### 测试完善
- 添加类型注册表保存/加载测试
- 添加带 id 映射的对象加载测试
- 测试覆盖率达到 87.7%

---

## 历史记录

### Object Manager 核心实现
- **全局对象管理器**：实现 `global_object_manager`，支持按类型名注册对象类型
- **类型注册机制**：类型 id 由内部递增分配（0, 1, 2, ...），最多支持 256 种类型
- **对象生命周期管理**：
  - `object_manager_create_object`：按类型名创建对象
  - `object_manager_release_object`：根据对象首字节的 type_id 自动释放
  - `object_manager_create_wrapped` / `object_manager_release_wrapped`：包装对象管理
- **序列化支持**：
  - `object_manager_save`：将对象保存到 ODB（写入 type_id + payload）
  - `object_manager_load`：从 ODB 加载对象（读取 type_id，调用对应类型的 load_fn）
- **类型查询**：`object_manager_get_type_name` 根据 type_id 获取类型名
- **计算方法**：`object_manager_calc` 支持对对象执行类型特定的计算方法（如 hash/校验）

### 从 RDB 到 ODB 的重构
- **架构重构**：从固定的 RDB 格式重构为灵活的 ODB（Object Database）
- **可插拔设计**：对象类型通过注册机制动态添加，而非硬编码
- **解耦设计**：`object_manager` 不负责直接注册内置类型（如 string），注册由外部模块按需完成

### 基础对象结构
- **latte_object_t**：核心对象结构
  - `type`：对象类型（8 bits）
  - `lru`：LRU 时间或 LFU 数据（24 bits）
  - `refcount`：引用计数（32 bits）
  - `ptr`：指向实际数据的指针
- **引用计数管理**：`latte_object_incr_ref_count` / `latte_object_decr_ref_count`

### 类型注册回调机制
每种对象类型需要提供以下回调函数：
- `object_create_fn`：创建对象
- `object_release_fn`：释放对象
- `object_save_fn`：保存对象到 ODB
- `object_load_fn`：从 ODB 加载对象
- `object_calc_fn`：计算方法（可选）

---

*格式说明：按时间倒序，[Unreleased] 为当前未发布变更。*
