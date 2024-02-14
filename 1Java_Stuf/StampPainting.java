import java.util.*;

public class StampPainting {
public static void main(String[] args) {
Scanner sc = new Scanner(System.in);
int t = sc.nextInt();
while (t-- > 0) {
int n = sc.nextInt();
char[][] canvas = new char[n][n];
for (int i = 0; i < n; i++) {
canvas[i] = sc.next().toCharArray();
}
int k = sc.nextInt();
char[][] stamp = new char[k][k];
for (int i = 0; i < k; i++) {
stamp[i] = sc.next().toCharArray();
}
boolean possible = isPossible(canvas, stamp);
System.out.println(possible ? "YES" : "NO");
}
sc.close();
}
private static boolean isPossible(char[][] canvas, char[][] stamp) {
    int n = canvas.length;
    int k = stamp.length;
    boolean[][] painted = new boolean[n][n];
    while (true) {
        boolean found = false;
        for (int i = 0; i <= n - k; i++) {
            for (int j = 0; j <= n - k; j++) {
                if (canStamp(canvas, stamp, i, j)) {
                    stamp(canvas, painted, stamp, i, j);
                    found = true;
                }
            }
        }
        if (!found) {
            break;
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (!painted[i][j] && canvas[i][j] == '*') {
                return false;
            }
        }
    }
    return true;
}

private static boolean canStamp(char[][] canvas, char[][] stamp, int x, int y) {
    int k = stamp.length;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (stamp[i][j] == '*' && canvas[x + i][y + j] != '*') {
                return false;
            }
        }
    }
    return true;
}

private static void stamp(char[][] canvas, boolean[][] painted, char[][] stamp, int x, int y) {
    int k = stamp.length;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (stamp[i][j] == '*') {
                canvas[x + i][y + j] = '*';
                painted[x + i][y + j] = true;
            }
        }
    }
}
}
