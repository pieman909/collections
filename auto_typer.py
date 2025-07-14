import tkinter as tk
import pyautogui
import time
import random
import sys
import re # Import regex for basic word splitting (not strictly needed for word counting now, but kept)

# --- Configuration ---
STARTUP_DELAY_SECONDS = 5 # Time to switch to the target window after pressing start
WPM = 600
# Calculate delay per character: 50 WPM is roughly 250 characters per minute (assuming 5 chars per word + space)
# 250 chars / 60 seconds = ~4.17 chars/sec
# Time per character = 1 / 4.17 = ~0.24 seconds
BASE_CHAR_INTERVAL = 60 / (WPM * 5) # Seconds per character
MOVE_LEFT_PIXELS = 600 # Pixels to move left after a newline
PERIOD_PAUSE_SECONDS_MIN = 10
PERIOD_PAUSE_SECONDS_MAX = 12

# Random delete/retype configuration
WORD_COUNT_TRIGGER_MIN = 50
WORD_COUNT_TRIGGER_MAX = 60
WORDS_TO_DELETE_MIN = 3
WORDS_TO_DELETE_MAX = 5
DELETE_RETYPE_PAUSE_SECONDS = 10

# --- GUI Setup ---
root = tk.Tk()
root.title("Automated Typing Program")

# --- Functions ---
def get_next_word_trigger():
    """Returns a random word count threshold between min and max."""
    return random.randint(WORD_COUNT_TRIGGER_MIN, WORD_COUNT_TRIGGER_MAX)

def start_typing():
    # Get text from the text widget, remove trailing newline from get()
    text_to_type = text_input.get("1.0", tk.END).strip()

    if not text_to_type:
        status_label.config(text="Please paste text into the box.", fg="red")
        return

    status_label.config(text=f"Starting in {STARTUP_DELAY_SECONDS} seconds. Switch to your target application now...", fg="orange")
    start_button.config(state=tk.DISABLED)
    root.update() # Update the GUI to show the message

    time.sleep(STARTUP_DELAY_SECONDS)

    status_label.config(text="Typing...", fg="blue")
    root.update()

    try:
        # Simulate clicking at the current mouse location to ensure focus
        # Get current mouse position
        current_x, current_y = pyautogui.position()
        pyautogui.click(current_x, current_y) # Click at the starting position
        time.sleep(0.1) # Short pause after clicking

        word_count = 0
        next_word_trigger = get_next_word_trigger()
        # recent_typed_chars will store the exact sequence of characters typed for the current word count cycle
        recent_typed_chars = []


        # Iterate through the text character by character
        char_index = 0
        while char_index < len(text_to_type):
            char = text_to_type[char_index]

            # Check for newline character '\n'
            if char == '\n':
                pyautogui.press('enter')
                status_label.config(text=f"Typing... (Moved Left after Newline)", fg="blue")
                root.update()
                time.sleep(0.1) # Small pause after typing newline

                # Move the mouse left
                pyautogui.moveRel(MOVE_LEFT_PIXELS, 0)
                time.sleep(0.1) # Small pause after moving

                # Simulate click again to ensure focus at the new location
                pyautogui.click()
                time.sleep(0.1) # Small pause after clicking

                # Reset word count, trigger, and recent chars after a newline/paragraph break
                word_count = 0
                next_word_trigger = get_next_word_trigger()
                recent_typed_chars = []

                char_index += 1
                continue

            # Check for period pause
            if char == '.':
                pyautogui.write(char)
                # Add the period to recent_typed_chars before pausing
                recent_typed_chars.append(char)
                pause_duration = random.uniform(PERIOD_PAUSE_SECONDS_MIN, PERIOD_PAUSE_SECONDS_MAX)
                status_label.config(text=f"Typing... (Paused {pause_duration:.2f}s)", fg="blue")
                root.update()
                time.sleep(pause_duration)
                char_index += 1
                continue

            # --- Handle random delete/retype Trigger ---
            # We increment word count when a space is encountered (signifying end of a word)
            # Check if current char is space and previous wasn't (to avoid double counting)
            if char.isspace() and char_index > 0 and not text_to_type[char_index-1].isspace():
                 word_count += 1

            if word_count >= next_word_trigger:
                # Trigger the delete/retype action

                # Determine how many words back to go for manipulation (3-5 words)
                num_words_to_manipulate = random.randint(WORDS_TO_DELETE_MIN, WORDS_TO_DELETE_MAX)

                # Find the substring of the last N words from recent_typed_chars
                # This requires iterating back to find word boundaries (spaces)
                chars_for_manipulation = []
                temp_word_count = 0
                # Iterate backwards through recent_typed_chars
                for i in range(len(recent_typed_chars) - 1, -1, -1):
                     current_recent_char = recent_typed_chars[i]
                     chars_for_manipulation.append(current_recent_char)

                     # If this character is a space and the previous wasn't (within this segment)
                     # check if we've now completed a word boundary count
                     # Need to be careful with leading/trailing spaces in the selected segment
                     # A simpler way: just count spaces encountered going backwards. N words means N-1 spaces + start boundary
                     if current_recent_char.isspace():
                         # Look ahead to see if the *next* char in the reversed sequence is not space
                         # This indicates the space is a separator, not a leading/trailing one of the segment
                         if i < len(recent_typed_chars) - 1 and not recent_typed_chars[i+1].isspace():
                             temp_word_count += 1

                     # Stop when we've gone back enough words (temp_word_count >= num_words_to_manipulate - 1 seems right?)
                     # This logic is still a bit tricky due to punctuation and spaces.
                     # Let's simplify: Just check if we've collected *enough non-space characters* that would form N words.
                     # Or better yet, find the index in recent_typed_chars that corresponds to the start of the Nth word back.

                # Let's try finding the start index more reliably
                manipulation_start_index = -1
                words_found_backwards = 0
                # Iterate backwards from the end of recent_typed_chars
                for i in range(len(recent_typed_chars) - 1, -1, -1):
                    # If the current char is NOT a space AND (it's the very last char OR the next char is a space)
                    # This indicates the END of a word going backwards
                    if not recent_typed_chars[i].isspace() and (i == len(recent_typed_chars) - 1 or recent_typed_chars[i+1].isspace()):
                         words_found_backwards += 1

                    if words_found_backwards == num_words_to_manipulate:
                        manipulation_start_index = i # This is the index of the first character of the Nth word back
                        break

                # --- Perform the delete/retype if we found enough words ---
                if manipulation_start_index != -1:
                    status_label.config(text="Typing... (Performing Delete/Retype)", fg="purple")
                    root.update()

                    # Get the exact string segment to manipulate
                    string_to_manipulate = "".join(recent_typed_chars[manipulation_start_index:])
                    total_chars_to_backspace = len(string_to_manipulate)

                    time.sleep(DELETE_RETYPE_PAUSE_SECONDS)

                    # Simulate backspacing
                    for _ in range(total_chars_to_backspace):
                        pyautogui.press('backspace')
                        # time.sleep(0.01) # Very small delay for visual effect, optional

                    # Pause
                    time.sleep(DELETE_RETYPE_PAUSE_SECONDS)

                    # Simulate clicking BEFORE re-typing - **This is a key fix attempt**
                    pyautogui.click()
                    time.sleep(0.1) # Small pause after clicking

                    # Simulate re-typing the words
                    pyautogui.write(string_to_manipulate)
                    time.sleep(0.1) # Small pause after retyping

                    # Remove manipulated characters from recent_typed_chars list
                    recent_typed_chars = recent_typed_chars[:manipulation_start_index]


                # Reset word count and set a new trigger regardless of whether manipulation happened
                # The word count reset here applies to the TRIGGER, not the typing index
                word_count = 0
                next_word_trigger = get_next_word_trigger()
                status_label.config(text="Typing...", fg="blue")
                root.update()


            # --- Type the current character (if not a special case handled above) ---
            # If the character was a newline or period, it was handled and char_index advanced
            # If it was a regular character or space, type it here

            # Only type if it wasn't already handled (newline/period 'continue' statements)
            # This check is implicit because if char was '\n' or '.', 'continue' would skip here

            # Add the current character to the recent_typed_chars list
            recent_typed_chars.append(char)

            pyautogui.write(char)

            # Apply the standard WPM interval (unless a period pause or delete/retype pause just happened)
            time.sleep(BASE_CHAR_INTERVAL)
            char_index += 1


        status_label.config(text="Typing complete!", fg="green")

    except pyautogui.FailSafeException:
        status_label.config(text="Operation aborted by user (Fail-Safe activated).", fg="red")
    except Exception as e:
        status_label.config(text=f"An error occurred: {e}", fg="red")
        print(f"An error occurred: {e}")
    finally:
        start_button.config(state=tk.NORMAL)
        root.update()


# --- GUI Elements ---
frame = tk.Frame(root, padx=10, pady=10)
frame.pack(padx=10, pady=10)

label = tk.Label(frame, text="Paste text here:")
label.pack(pady=5)

text_input = tk.Text(frame, height=10, width=70, wrap=tk.WORD)
text_input.pack(pady=5)

start_button = tk.Button(frame, text="Start Typing", command=start_typing)
start_button.pack(pady=10)

status_label = tk.Label(frame, text="Ready", fg="black")
status_label.pack(pady=5)

# --- Run GUI ---
root.mainloop()
