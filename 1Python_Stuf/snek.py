import pygame
import time
import random

pygame.init()

white = (255,255,255)
black = (0,0,0)
red = (255,0,0)
green = (0,255,0)
blue = (0,0,255)
purple = (128,0,128)
yellow = (255, 234, 0)

display_width = 800
display_height = 600

clock = pygame.time.Clock()
fps = 5
block_size = 50

font = pygame.font.SysFont(None, 25)

def snake(block_size, snakeList, snakeHead, lead_x, lead_y):
    for XnY in snakeList:
        pygame.draw.rect(gameDisplay, white, [XnY[0],XnY[1],block_size,block_size])
        pygame.draw.rect(gameDisplay, blue, [lead_x,lead_y,block_size,block_size])

def message_to_screen(msg,color,x,y):
    screen_text = font.render(msg, True, color)
    gameDisplay.blit(screen_text, [x,y])

gameDisplay = pygame.display.set_mode((800,600))
pygame.display.set_caption("Jew Simulator")

def gameLoop():
    gameExit = False
    gameOver = False

    lead_x = display_width/2
    lead_y = display_height/2
    lead_x_change = 0
    lead_y_change = 0

    snakeList = []
    snakeLength = 1

    score = 0

    randAppleX = round(random.randrange(0, display_width - block_size)/block_size)*block_size
    randAppleY = round(random.randrange(0, display_height - block_size)/block_size)*block_size

    while not gameExit:
        while gameOver == True:
            gameDisplay.fill(red)
            message_to_screen("Oy vey! You ate your own shekels! Press C to support Israel's noble cause or Q to be a bad goyi- I mean anti-semite.", white, 180,280)
            message_to_screen(''.join(["You have ",str(score), "shekels"]), white, 300, 325)
            pygame.display.update()

            for event in pygame.event.get():
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_c:
                        gameLoop()
                    if event.key == pygame.K_q:
                        gameExit = True
                        gameOver = False
                elif event.type == pygame.QUIT:
                    gameExit = True
                    gameOver = False

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                gameExit = True
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_LEFT:
                    lead_x_change = -block_size
                    lead_y_change = 0
                elif event.key == pygame.K_RIGHT:
                    lead_x_change = block_size
                    lead_y_change = 0
                elif event.key == pygame.K_DOWN:
                    lead_y_change = block_size
                    lead_x_change = 0
                elif event.key == pygame.K_UP:
                    lead_y_change = -block_size
                    lead_x_change = 0

        if lead_x >= display_width or lead_x <0 or lead_y >= display_height or lead_y <0:
            gameOver = True

        lead_x += lead_x_change
        lead_y += lead_y_change
        gameDisplay.fill(black)
        message_to_screen(''.join(["Muh Shekels: ",str(score)]), black, 10,10)
        pygame.draw.rect(gameDisplay, yellow, [randAppleX, randAppleY, block_size, block_size])

        snakeHead = []
        snakeHead.append(lead_x)
        snakeHead.append(lead_y)
        snakeList.append(snakeHead)

        if len(snakeList) > snakeLength:
            del snakeList[0]
        for eachSegment in snakeList[:-1]:
            if eachSegment == snakeHead:
                gameOver = True

        snake(block_size, snakeList, snakeHead, lead_x, lead_y)

        if lead_x == randAppleX and lead_y == randAppleY:
            randAppleX = round(random.randrange(0, display_width - block_size)/block_size)*block_size
            randAppleY = round(random.randrange(0, display_height - block_size)/block_size)*block_size
            snakeLength += 1
            score += 1
            message_to_screen(''.join(["Muh Shekels: ",str(score)]), black, 10,10)

        pygame.display.update()

        clock.tick(fps)

    pygame.quit()

gameLoop()
