#ifndef ROCKET_COMMON_CONFIG_H
#define ROCKET_COMMON_CONFIG_H

namespace rocket {
class Config {
   public:
    Config(const char* xmlfile);

    static Config* GetGlobalConfig();
    static void SetGlobalConfig(const char* xmlfile);   // 读取 xml 文件

    std::string m_log_level;

   private:
    // std::map<std::string, std::string> m_config_values;

};
}  // namespace rocket

#endif
