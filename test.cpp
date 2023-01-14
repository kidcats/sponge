#include <cstring>
#include <iostream>
#include <string>

int main() {
  std::string str = "hello";
  size_t strLen = str.length();
  // 将string强制转换为unsigned char数组
  unsigned char bytes[strLen];
memcpy(bytes, str.data(), strLen);
  // 访问第2个字节
  std::cout << std::hex << std::uppercase << (int)bytes[1] << std::endl;  // 输出65（即字符'e'的ASCII码）
    std::cout<<"string size "<< str.size()<< " bytes size : " << bytes << std::endl;
  // 修改第4个字节
  bytes[3] = 0x70;  // 即字符'p'的ASCII码

  // 输出修改后的字符串
  std::cout << str << std::endl;  // 输出helpo

  return 0;
}
