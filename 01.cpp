#include<iostream>
#include<string>
using namespace std;
class HugeInteger {
public:
	void input();
	void output();
	int* add(HugeInteger a);
	void subtract();
	int* arr = new int[40];
	int len = 0;
};
void HugeInteger::input() {
	int n;
	while (cin >> n) {
		arr[len] = n;
		len++;
		if(i==40)
			break;
	}
}
void HugeInteger::output() {
	for (int i = 0; i < len; i++) {
		cout << arr[i];
	}
}
int* HugeInteger::add(HugeInteger a) {
	int result1 = (a.len > this->len) ? a.len : this->len;
	HugeInteger big = (result1 == a.len) ? a : *this;
	int result2 = (a.len > this->len) ? this->len : a.len;
	HugeInteger small = (result2 == a.len) ? a : *this;
	int* sum = new int[40];
	for (int i = result2; i > 0; i--) {
		sum[i]= small.arr[i] + big.arr[i];
	}
	for (int i = result1 - result2; i >= 0; i--) {
		sum[i] = big.arr[i];
	}

}