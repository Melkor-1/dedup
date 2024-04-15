dedup: dedup.cpp
	clang++-18 -std=c++17 -Wall -Werror -Wpedantic -O2 dedup.cpp -o dedup -lcrypto

clean:
	$(RM) dedup


