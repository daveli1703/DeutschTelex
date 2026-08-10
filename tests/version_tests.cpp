#include "deutschtelex_version.h"

#include <iostream>

int main() {
    int passed = 0;
    int failed = 0;
    const auto check = [&passed, &failed](const bool condition, const char* const name) {
        if (condition) {
            ++passed;
        } else {
            ++failed;
            std::cerr << "FAILED: " << name << '\n';
        }
    };

    check(deutschtelex::version::kMajor == 0 &&
              deutschtelex::version::kMinor == 6 &&
              deutschtelex::version::kPatch == 0,
          "numeric release version is 0.6.0");
    check(deutschtelex::version::kString == "0.6.0",
          "display release version is 0.6.0");
    check(deutschtelex::version::kPortableDirectoryName ==
              "DeutschTelex-0.6.0-win64",
          "portable directory name is stable");
    check(deutschtelex::version::kPortableArchiveName ==
              "DeutschTelex-0.6.0-win64-portable.zip",
          "portable archive name is stable");
    check(deutschtelex::version::kInstallerName ==
              "DeutschTelex-0.6.0-win64-setup.exe",
          "installer artifact name is stable");

    std::cout << passed << " tests passed; " << failed << " tests failed.\n";
    return failed == 0 ? 0 : 1;
}
