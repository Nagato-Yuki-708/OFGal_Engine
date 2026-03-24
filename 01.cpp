#include<iostream>
#include<string>
using namespace std;
class Complex {
public:
	Complex(int a = 0, int b = 0) :realPart(a), imaginaryPart(b) {}
	friend Complex operator+(const Complex& a,const Complex& b);
	friend Complex operator-(const Complex& a, const Complex& b);
	friend Complex operator*(const Complex& a, const Complex& b);
	friend Complex operator/(const Complex& a, const Complex& b);
	friend istream& operator >>(istream& is, Complex& a);
	friend ostream& operator<<(ostream& os, Complex& a);

private:
	double realPart;
	double imaginaryPart;
};

Complex operator+(const Complex& a, const Complex& b) {
	Complex temp;
	temp.realPart = a.realPart + b.realPart;
	temp.imaginaryPart = a.imaginaryPart + b.imaginaryPart;
	return temp;
}
Complex operator-(const Complex& a, const Complex& b) {
	Complex temp;
	temp.realPart = a.realPart - b.realPart;
	temp.imaginaryPart = a.imaginaryPart - b.imaginaryPart;
	return temp;
}
Complex operator*(const Complex& a, const Complex& b) {
	Complex temp;
	temp.realPart = a.realPart * b.realPart - a.imaginaryPart * b.imaginaryPart;
	temp.imaginaryPart = a.realPart * b.imaginaryPart + a.imaginaryPart * b.realPart;
	return temp;
}
Complex operator/(const Complex& a, const Complex& b) {
	Complex temp;
	double denominator = b.realPart * b.realPart + b.imaginaryPart * b.imaginaryPart;
	temp.realPart = (a.realPart * b.realPart + a.imaginaryPart * b.imaginaryPart) / denominator;
	temp.imaginaryPart = (a.imaginaryPart * b.realPart - a.realPart * b.imaginaryPart) / denominator;
	return temp;
}
istream& operator >>(istream& is, Complex& a) {
	string s;
	getline(is, s);
	int pos = s.find(',');
	a.realPart = stod(s.substr(0, pos));
	a.imaginaryPart = stod(s.substr(pos + 1));
	return is;
}
ostream& operator<<(ostream& os, Complex& a) {
	os << '(' << a.realPart << ',' << a.imaginaryPart << ')';
	return os;
}
int main() {
	string s, z;
	Complex a;
	double c, d;
	cin >> a;
	while (getline(cin, s)) {
		if (s.empty()) continue;
		int pos1 = s.find(',');
		int pos2 = s.find(',', pos1 + 1);
		z = s.substr(0, pos1);
		c = stod(s.substr(pos1 + 1, pos2-pos1-1));
		d = stod(s.substr(pos2 + 1));
		Complex b(c, d);
		if (z == "add") a = a + b;
		else if (z == "sub") a = a - b;
		else if (z == "mul") a = a * b;
		else if (z == "div") a = a / b;
	}
	cout << a << endl;
}