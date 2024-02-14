/*
Instructions:

    If you want to run it in your Terminal/CLI:
        1. Install any version of JDK if you didn't already

        2. CD into the project folderin the terminal/CLI

        3. insert 'javac battleship.java' into the terminal/CLI to compile

        4. insert 'java battleship' to run the main fuction and start the program

        5. Play to your heart's content

    If you don't wanna bother
        1. Copy-paste into the editor of your choice

        2. Click run
*/

import java.util.*;

public class Battleship
{
    public static Scanner reader = new Scanner(System.in);
      
    public static void main(String[] args)
    {
        System.out.println("JAVA BATTLESHIP");
        
        System.out.println("\nPlayer SETUP:");
        Player userPlayer = new Player();
        setup(userPlayer);
        
        System.out.println("Computer SETUP...DONE...PRESS ENTER TO CONTINUE...");
        reader.nextLine();
        reader.nextLine();
        Player computer = new Player();
        setupComputer(computer);
        System.out.println("\nCOMPUTER GRID (FOR DEBUG)...");
        computer.playerGrid.printShips();
        
        String result = "";
        while(true)
        {
            System.out.println(result);
            System.out.println("\nUSER MAKE GUESS:");
            result = askForGuess(userPlayer, computer);
            
            if (userPlayer.playerGrid.hasLost())
            {
                System.out.println("COMP HIT!...USER LOSES");
                break;
            }
            else if (computer.playerGrid.hasLost())
            {
                System.out.println("HIT!...COMPUTER LOSES");
                break;
            }
            
            System.out.println("\nCOMPUTER IS MAKING GUESS...");
              
              
            compMakeGuess(computer, userPlayer);
        }
    }
    
    private static void compMakeGuess(Player comp, Player user)
    {
        Randomizer rand = new Randomizer();
        int row = rand.nextInt(0, 9);
        int col = rand.nextInt(0, 9);
        
        // While computer already guessed this posiiton, make a new random guess
        while (comp.oppGrid.alreadyGuessed(row, col))
        {
            row = rand.nextInt(0, 9);
            col = rand.nextInt(0, 9);
        }
        
        if (user.playerGrid.hasShip(row, col))
        {
            comp.oppGrid.markHit(row, col);
            user.playerGrid.markHit(row, col);
            System.out.println("COMP HIT AT " + convertIntToLetter(row) + convertCompColToRegular(col));
        }
        else
        {
            comp.oppGrid.markMiss(row, col);
            user.playerGrid.markMiss(row, col);
            System.out.println("COMP MISS AT " + convertIntToLetter(row) + convertCompColToRegular(col));
        }
        
        
        System.out.println("\nYOUR BOARD...PRESS ENTER TO CONTINUE...");
        reader.nextLine();
        user.playerGrid.printCombined();
        System.out.println("PRESS ENTER TO CONTINUE...");
        reader.nextLine();
    }

    private static String askForGuess(Player p, Player opp)
    {
        System.out.println("Viewing My Guesses:");
        p.oppGrid.printStatus();
        
        int row = -1;
        int col = -1;
        
        String oldRow = "Z";
        int oldCol = -1;
        
        while(true)
        {
            System.out.print("Type in row (A-J): ");
            String userInputRow = reader.next();
            userInputRow = userInputRow.toUpperCase();
            oldRow = userInputRow;
            row = convertLetterToInt(userInputRow);
                    
            System.out.print("Type in column (1-10): ");
            col = reader.nextInt();
            oldCol = col;
            col = convertUserColToProCol(col);
                    
            //System.out.println("DEBUG: " + row + col);
                    
            if (col >= 0 && col <= 9 && row != -1)
                break;
                    
            System.out.println("Invalid location!");
        }
        
        if (opp.playerGrid.hasShip(row, col))
        {
            p.oppGrid.markHit(row, col);
            opp.playerGrid.markHit(row, col);
            return "** USER HIT AT " + oldRow + oldCol + " **";
        }
        else
        {
            p.oppGrid.markMiss(row, col);
            opp.playerGrid.markMiss(row, col);
            return "** USER MISS AT " + oldRow + oldCol + " **";
        }
    }
    
    private static void setup(Player p)
    {
        p.playerGrid.printShips();
        System.out.println();
        int counter = 1;
        int normCounter = 0;
        while (p.numOfShipsLeft() > 0)
        {
            for (Ship s: p.ships)
            {
                System.out.println("\nShip #" + counter + ": Length-" + s.getLength());
                int row = -1;
                int col = -1;
                int dir = -1;
                while(true)
                {
                    System.out.print("Type in row (A-J): ");
                    String userInputRow = reader.next();
                    userInputRow = userInputRow.toUpperCase();
                    row = convertLetterToInt(userInputRow);
                    
                    System.out.print("Type in column (1-10): ");
                    col = reader.nextInt();
                    col = convertUserColToProCol(col);
                    
                    System.out.print("Type in direction (0-H, 1-V): ");
                    dir = reader.nextInt();
                    
                    //System.out.println("DEBUG: " + row + col + dir);
                    
                    if (col >= 0 && col <= 9 && row != -1 && dir != -1) // Check valid input
                    {
                        if (!hasErrors(row, col, dir, p, normCounter)) // Check if errors will produce (out of bounds)
                        {
                            break;
                        }
                    }
    
                    System.out.println("Invalid location!");
                }
            
                //System.out.println("FURTHER DEBUG: row = " + row + "; col = " + col);
                p.ships[normCounter].setLocation(row, col);
                p.ships[normCounter].setDirection(dir);
                p.playerGrid.addShip(p.ships[normCounter]);
                p.playerGrid.printShips();
                System.out.println();
                System.out.println("You have " + p.numOfShipsLeft() + " remaining ships to place.");
                
                normCounter++;
                counter++;
            }
        }
    }

    private static void setupComputer(Player p)
    {
        System.out.println();
        int counter = 1;
        int normCounter = 0;
        
        Randomizer rand = new Randomizer();
        
        while (p.numOfShipsLeft() > 0)
        {
            for (Ship s: p.ships)
            {
                int row = rand.nextInt(0, 9);
                int col = rand.nextInt(0, 9);
                int dir = rand.nextInt(0, 1);
                
                //System.out.println("DEBUG: row-" + row + "; col-" + col + "; dir-" + dir);
                
                while (hasErrorsComp(row, col, dir, p, normCounter)) // while the random nums make error, start again
                {
                    row = rand.nextInt(0, 9);
                    col = rand.nextInt(0, 9);
                    dir = rand.nextInt(0, 1);
                    //System.out.println("AGAIN-DEBUG: row-" + row + "; col-" + col + "; dir-" + dir);
                }
                
                //System.out.println("FURTHER DEBUG: row = " + row + "; col = " + col);
                p.ships[normCounter].setLocation(row, col);
                p.ships[normCounter].setDirection(dir);
                p.playerGrid.addShip(p.ships[normCounter]);
                
                normCounter++;
                counter++;
            }
        }
    }
    
    private static boolean hasErrors(int row, int col, int dir, Player p, int count)
    {
        //System.out.println("DEBUG: count arg is " + count);
        
        int length = p.ships[count].getLength();
        
        // Check if off grid - Horizontal
        if (dir == 0)
        {
            int checker = length + col;
            //System.out.println("DEBUG: checker is " + checker);
            if (checker > 10)
            {
                System.out.println("SHIP DOES NOT FIT");
                return true;
            }
        }
        
        // Check if off grid - Vertical
        if (dir == 1) // VERTICAL
        {
            int checker = length + row;
            //System.out.println("DEBUG: checker is " + checker);
            if (checker > 10)
            {
                System.out.println("SHIP DOES NOT FIT");
                return true;
            }
        }
            
        // Check if overlapping with another ship
        if (dir == 0) // Hortizontal
        {
            // For each location a ship occupies, check if ship is already there
            for (int i = col; i < col+length; i++)
            {
                //System.out.println("DEBUG: row = " + row + "; col = " + i);
                if(p.playerGrid.hasShip(row, i))
                {
                    System.out.println("THERE IS ALREADY A SHIP AT THAT LOCATION");
                    return true;
                }
            }
        }
        else if (dir == 1) // Vertical
        {
            // For each location a ship occupies, check if ship is already there
            for (int i = row; i < row+length; i++)
            {
                //System.out.println("DEBUG: row = " + row + "; col = " + i);
                if(p.playerGrid.hasShip(i, col))
                {
                    System.out.println("THERE IS ALREADY A SHIP AT THAT LOCATION");
                    return true;
                }
            }
        }
        
        return false;
    }
    
    private static boolean hasErrorsComp(int row, int col, int dir, Player p, int count)
    {
        //System.out.println("DEBUG: count arg is " + count);
        
        int length = p.ships[count].getLength();
        
        // Check if off grid - Horizontal
        if (dir == 0)
        {
            int checker = length + col;
            //System.out.println("DEBUG: checker is " + checker);
            if (checker > 10)
            {
                return true;
            }
        }
        
        // Check if off grid - Vertical
        if (dir == 1) // VERTICAL
        {
            int checker = length + row;
            //System.out.println("DEBUG: checker is " + checker);
            if (checker > 10)
            {
                return true;
            }
        }
            
        // Check if overlapping with another ship
        if (dir == 0) // Hortizontal
        {
            // For each location a ship occupies, check if ship is already there
            for (int i = col; i < col+length; i++)
            {
                //System.out.println("DEBUG: row = " + row + "; col = " + i);
                if(p.playerGrid.hasShip(row, i))
                {
                    return true;
                }
            }
        }
        else if (dir == 1) // Vertical
        {
            // For each location a ship occupies, check if ship is already there
            for (int i = row; i < row+length; i++)
            {
                //System.out.println("DEBUG: row = " + row + "; col = " + i);
                if(p.playerGrid.hasShip(i, col))
                {
                    return true;
                }
            }
        }
        
        return false;
    }


    /*HELPER METHODS*/
    private static int convertLetterToInt(String val)
    {
        int toReturn = -1;
        switch (val)
        {
            case "A":   toReturn = 0;
                        break;
            case "B":   toReturn = 1;
                        break;
            case "C":   toReturn = 2;
                        break;
            case "D":   toReturn = 3;
                        break;
            case "E":   toReturn = 4;
                        break;
            case "F":   toReturn = 5;
                        break;
            case "G":   toReturn = 6;
                        break;
            case "H":   toReturn = 7;
                        break;
            case "I":   toReturn = 8;
                        break;
            case "J":   toReturn = 9;
                        break;
            default:    toReturn = -1;
                        break;
        }
        
        return toReturn;
    }
    
    private static String convertIntToLetter(int val)
    {
        String toReturn = "Z";
        switch (val)
        {
            case 0:   toReturn = "A";
                        break;
            case 1:   toReturn = "B";
                        break;
            case 2:   toReturn = "C";
                        break;
            case 3:   toReturn = "D";
                        break;
            case 4:   toReturn = "E";
                        break;
            case 5:   toReturn = "F";
                        break;
            case 6:   toReturn = "G";
                        break;
            case 7:   toReturn = "H";
                        break;
            case 8:   toReturn = "I";
                        break;
            case 9:   toReturn = "J";
                        break;
            default:    toReturn = "Z";
                        break;
        }
        
        return toReturn;
    }
    
    private static int convertUserColToProCol(int val)
    {
        int toReturn = -1;
        switch (val)
        {
            case 1:   toReturn = 0;
                        break;
            case 2:   toReturn = 1;
                        break;
            case 3:   toReturn = 2;
                        break;
            case 4:   toReturn = 3;
                        break;
            case 5:   toReturn = 4;
                        break;
            case 6:   toReturn = 5;
                        break;
            case 7:   toReturn = 6;
                        break;
            case 8:   toReturn = 7;
                        break;
            case 9:   toReturn = 8;
                        break;
            case 10:   toReturn = 9;
                        break;
            default:    toReturn = -1;
                        break;
        }
        
        return toReturn;
    }
    
    private static int convertCompColToRegular(int val)
    {
        int toReturn = -1;
        switch (val)
        {
            case 0:   toReturn = 1;
                        break;
            case 1:   toReturn = 2;
                        break;
            case 2:   toReturn = 3;
                        break;
            case 3:   toReturn = 4;
                        break;
            case 4:   toReturn = 5;
                        break;
            case 5:   toReturn = 6;
                        break;
            case 6:   toReturn = 7;
                        break;
            case 7:   toReturn = 8;
                        break;
            case 8:   toReturn = 9;
                        break;
            case 9:   toReturn = 10;
                        break;
            default:    toReturn = -1;
                        break;
        }
        
        return toReturn;
    }
}


class Grid
{
    private Location[][] grid;
    private int points;

    // Constants for number of rows and columns.
    public static final int NUM_ROWS = 10;
    public static final int NUM_COLS = 10;
    
    public Grid()
    {
        if (NUM_ROWS > 26)
        {
            throw new IllegalArgumentException("ERROR! NUM_ROWS CANNOT BE > 26");
        }
        
        grid = new Location[NUM_ROWS][NUM_COLS];
        
        for (int row = 0; row < grid.length; row++)
        {
            for (int col = 0; col < grid[row].length; col++)
            {
                Location tempLoc = new Location();
                grid[row][col] = tempLoc;
            }
        }
    }
    
    // Mark a hit in this location by calling the markHit method
    // on the Location object.  
    public void markHit(int row, int col)
    {
        grid[row][col].markHit();
        points++;
    }
    
    // Mark a miss on this location.    
    public void markMiss(int row, int col)
    {
        grid[row][col].markMiss();
    }
    
    // Set the status of this location object.
    public void setStatus(int row, int col, int status)
    {
        grid[row][col].setStatus(status);
    }
    
    // Get the status of this location in the grid  
    public int getStatus(int row, int col)
    {
        return grid[row][col].getStatus();
    }
    
    // Return whether or not this Location has already been guessed.
    public boolean alreadyGuessed(int row, int col)
    {
        return !grid[row][col].isUnguessed();
    }
    
    // Set whether or not there is a ship at this location to the val   
    public void setShip(int row, int col, boolean val)
    {
        grid[row][col].setShip(val);
    }
    
    // Return whether or not there is a ship here   
    public boolean hasShip(int row, int col)
    {
        return grid[row][col].hasShip();
    }
    
    // Get the Location object at this row and column position
    public Location get(int row, int col)
    {
        return grid[row][col];
    }
    
    // Return the number of rows in the Grid
    public int numRows()
    {
        return NUM_ROWS;
    }

    // Return the number of columns in the grid
    public int numCols()
    {
        return NUM_COLS;
    }
    
    public void printStatus()
    {
        generalPrintMethod(0);
    }
    
    public void printShips()
    {
        generalPrintMethod(1);
    }
    
    public void printCombined()
    {
        generalPrintMethod(2);
    }
    
    public boolean hasLost()
    {
        if (points >= 17)
            return true;
        else
            return false;
    }
    
    public void addShip(Ship s)
    {
        int row = s.getRow();
        int col = s.getCol();
        int length = s.getLength();
        int dir = s.getDirection();
        
        if (!(s.isDirectionSet()) || !(s.isLocationSet()))
            throw new IllegalArgumentException("ERROR! Direction or Location is unset/default");
        
        // 0 - hor; 1 - ver
        if (dir == 0) // Hortizontal
        {
            for (int i = col; i < col+length; i++)
            {
                //System.out.println("DEBUG: row = " + row + "; col = " + i);
                grid[row][i].setShip(true);
                grid[row][i].setLengthOfShip(length);
                grid[row][i].setDirectionOfShip(dir);
            }
        }
        else if (dir == 1) // Vertical
        {
            for (int i = row; i < row+length; i++)
            {
                //System.out.println("DEBUG: row = " + row + "; col = " + i);
                grid[i][col].setShip(true);
                grid[i][col].setLengthOfShip(length);
                grid[i][col].setDirectionOfShip(dir);
            }
        }
    }
    
    // Type: 0 for status, 1 for ships, 2 for combined
    private void generalPrintMethod(int type)
    {
        System.out.println();
        // Print columns (HEADER)
        System.out.print("  ");
        for (int i = 1; i <= NUM_COLS; i++)
        {
            System.out.print(i + " ");
        }
        System.out.println();
        
        // Print rows
        int endLetterForLoop = (NUM_ROWS - 1) + 65;
        for (int i = 65; i <= endLetterForLoop; i++)
        {
            char theChar = (char) i;
            System.out.print(theChar + " ");
            
            for (int j = 0; j < NUM_COLS; j++)
            {
                if (type == 0) // type == 0; status
                {
                    if (grid[switchCounterToIntegerForArray(i)][j].isUnguessed())
                        System.out.print("- ");
                    else if (grid[switchCounterToIntegerForArray(i)][j].checkMiss())
                        System.out.print("O ");
                    else if (grid[switchCounterToIntegerForArray(i)][j].checkHit())
                        System.out.print("X ");
                }
                else if (type == 1) // type == 1; ships
                {
                    if (grid[switchCounterToIntegerForArray(i)][j].hasShip())
                    {
                        // System.out.print("X ");
                        if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 2)
                        {
                            System.out.print("D ");
                        }
                        else if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 3)
                        {
                            System.out.print("C ");
                        }
                        else if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 4)
                        {
                            System.out.print("B ");
                        }
                        else if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 5)
                        {
                            System.out.print("A ");
                        }
                    }
                        
                    else if (!(grid[switchCounterToIntegerForArray(i)][j].hasShip()))
                    {
                        System.out.print("- ");
                    }
                        
                }
                else // type == 2; combined
                {
                    if (grid[switchCounterToIntegerForArray(i)][j].checkHit())
                        System.out.print("X ");
                    else if (grid[switchCounterToIntegerForArray(i)][j].hasShip())
                    {
                        // System.out.print("X ");
                        if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 2)
                        {
                            System.out.print("D ");
                        }
                        else if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 3)
                        {
                            System.out.print("C ");
                        }
                        else if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 4)
                        {
                            System.out.print("B ");
                        }
                        else if (grid[switchCounterToIntegerForArray(i)][j].getLengthOfShip() == 5)
                        {
                            System.out.print("A ");
                        }
                    }
                    else if (!(grid[switchCounterToIntegerForArray(i)][j].hasShip()))
                    {
                        System.out.print("- ");
                    }  
                }
            }
            System.out.println();
        }
    }
    
    public int switchCounterToIntegerForArray (int val)
    {
        int toReturn = -1;
        switch (val)
        {
            case 65:    toReturn = 0;
                        break;
            case 66:    toReturn = 1;
                        break;
            case 67:    toReturn = 2;
                        break;
            case 68:    toReturn = 3;
                        break;
            case 69:    toReturn = 4;
                        break;
            case 70:    toReturn = 5;
                        break;
            case 71:    toReturn = 6;
                        break;
            case 72:    toReturn = 7;
                        break;
            case 73:    toReturn = 8;
                        break;
            case 74:    toReturn = 9;
                        break;
            case 75:    toReturn = 10;
                        break;
            case 76:    toReturn = 11;
                        break;
            case 77:    toReturn = 12;
                        break;
            case 78:    toReturn = 13;
                        break;
            case 79:    toReturn = 14;
                        break;
            case 80:    toReturn = 15;
                        break;
            case 81:    toReturn = 16;
                        break;
            case 82:    toReturn = 17;
                        break;
            case 83:    toReturn = 18;
                        break;
            case 84:    toReturn = 19;
                        break;
            case 85:    toReturn = 20;
                        break;
            case 86:    toReturn = 21;
                        break;
            case 87:    toReturn = 22;
                        break;
            case 88:    toReturn = 23;
                        break;
            case 89:    toReturn = 24;
                        break;
            case 90:    toReturn = 25;
                        break;
            default:    toReturn = -1;
                        break;
        }
        
        if (toReturn == -1)
        {
            throw new IllegalArgumentException("ERROR OCCURED IN SWITCH");
        }
        else
        {
            return toReturn;
        }
    }   
}


class Location
{
    // Global Vars
    public static final int UNGUESSED = 0;
    public static final int HIT = 1;
    public static final int MISSED = 2;
    
    // Instance Variables
    private boolean hasShip;
    private int status;
    private int lengthOfShip;
    private int directionOfShip;
    
    // Location constructor. 
    public Location()
    {
        // Set initial values
        status = 0;
        hasShip = false;
        lengthOfShip = -1;
        directionOfShip = -1;
    }

    // Was this Location a hit?
    public boolean checkHit()
    {
        if (status == HIT)
            return true;
        else
            return false;
    }

    // Was this location a miss?
    public boolean checkMiss()
    {
        if (status == MISSED)
            return true;
        else
            return false;
    }

    // Was this location unguessed?
    public boolean isUnguessed()
    {
        if (status == UNGUESSED)
            return true;
        else
            return false;
    }

    // Mark this location a hit.
    public void markHit()
    {
        setStatus(HIT);
    }

    // Mark this location a miss.
    public void markMiss()
    {
        setStatus(MISSED);
    }

    // Return whether or not this location has a ship.
    public boolean hasShip()
    {
        return hasShip;
    }

    // Set the value of whether this location has a ship.
    public void setShip(boolean val)
    {
        this.hasShip = val;
    }

    // Set the status of this Location.
    public void setStatus(int status)
    {
        this.status = status;
    }

    // Get the status of this Location.
    public int getStatus()
    {
        return status;
    }
    
    public int getLengthOfShip()
    {
        return lengthOfShip; 
    }
    
    public void setLengthOfShip(int val)
    {
        lengthOfShip = val;
    }
    
    public int getDirectionOfShip()
    {
        return directionOfShip; 
    }
    
    public void setDirectionOfShip(int val)
    {
        directionOfShip = val;
    }
}


class Player
{
    // These are the lengths of all of the ships.
    private static final int[] SHIP_LENGTHS = {2, 3, 3, 4, 5};
    private static final int NUM_OF_SHIPS = 5;
    
    public Ship[] ships;
    public Grid playerGrid;
    public Grid oppGrid;
    
    public Player()
    {
        if (NUM_OF_SHIPS != 5) // Num of ships must be 5
        {
            throw new IllegalArgumentException("ERROR! Num of ships must be 5");
        }
        
        ships = new Ship[NUM_OF_SHIPS];
        for (int i = 0; i < NUM_OF_SHIPS; i++)
        {
            Ship tempShip = new Ship(SHIP_LENGTHS[i]);
            ships[i] = tempShip;
        }
        
        playerGrid = new Grid();
        oppGrid = new Grid();
    }
    
    public void addShips()
    {
        for (Ship s: ships)
        {
            playerGrid.addShip(s);
        }
    }
    
    public int numOfShipsLeft()
    {
        int counter = 5;
        for (Ship s: ships)
        {
            if (s.isLocationSet() && s.isDirectionSet())
                counter--;
        }
        
        return counter;
        
    }
    
    public void chooseShipLocation(Ship s, int row, int col, int direction)
    {
        s.setLocation(row, col);
        s.setDirection(direction);
        playerGrid.addShip(s);
    }
}


class Randomizer
{

 public static Random theInstance = null;
 
 public Randomizer(){
  
 }
 
 public static Random getInstance(){
  if(theInstance == null){
   theInstance = new Random();
  }
  return theInstance;
 }
 
 public static boolean nextBoolean(){
  return Randomizer.getInstance().nextBoolean();
 }

 public static boolean nextBoolean(double probability){
  return Randomizer.nextDouble() < probability;
 }
 
 public static int nextInt(){
  return Randomizer.getInstance().nextInt();
 }

 public static int nextInt(int n){
  return Randomizer.getInstance().nextInt(n);
 }

 public static int nextInt(int min, int max){
  return min + Randomizer.nextInt(max - min + 1);
 }

 public static double nextDouble(){
  return Randomizer.getInstance().nextDouble();
 }

 public static double nextDouble(double min, double max){
  return min + (max - min) * Randomizer.nextDouble();
 }

 
}


class Ship
{
    /* Instance Variables */
    private int row;
    private int col;
    private int length;
    private int direction;
    
    // Direction Constants
    public static final int UNSET = -1;
    public static final int HORIZONTAL = 0;
    public static final int VERTICAL = 1;
    
    // Constructor
    public Ship(int length)
    {
        this.length = length;
        this.row = -1;
        this.col = -1;
        this.direction = UNSET;
    }
    
    // Has the location been init
    public boolean isLocationSet()
    {
        if (row == -1 || col == -1)
            return false;
        else
            return true;
    }
    
    // Has the direction been init
    public boolean isDirectionSet()
    {
        if (direction == UNSET)
            return false;
        else
            return true;
    }
    
    // Set the location of the ship
    public void setLocation(int row, int col)
    {
        this.row = row;
        this.col = col;
    }
    
    // Set the direction of the ship
    public void setDirection(int direction)
    {
        if (direction != UNSET && direction != HORIZONTAL && direction != VERTICAL)
            throw new IllegalArgumentException("Invalid direction. It must be -1, 0, or 1");
        this.direction = direction;
    }
    
    // Getter for the row value
    public int getRow()
    {
        return row;
    }
    
    // Getter for the column value
    public int getCol()
    {
        return col;
    }
    
    // Getter for the length of the ship
    public int getLength()
    {
        return length;
    }
    
    // Getter for the direction
    public int getDirection()
    {
        return direction;
    }
    
    // Helper method to get a string value from the direction
    private String directionToString()
    {
        if (direction == UNSET)
            return "UNSET";
        else if (direction == HORIZONTAL)
            return "HORIZONTAL";
        else
            return "VERTICAL";
    }
    
    // toString value for this Ship
    public String toString()
    {
        return "Ship: " + getRow() + ", " + getCol() + " with length " + getLength() + " and direction " + directionToString();
    }
}
