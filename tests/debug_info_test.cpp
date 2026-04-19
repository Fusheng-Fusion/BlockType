// 测试调试信息生成
int add(int a, int b) {
    return a + b;
}

struct Point {
    int x;
    int y;
};

int main() {
    Point p;
    p.x = 10;
    p.y = 20;
    
    int result = add(p.x, p.y);
    return result;
}
