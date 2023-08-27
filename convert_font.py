from PIL import Image
import os.path

def main():
	font_height = 8
	font_width = 5

	font = Image.open(os.path.join('..', 'font.png'))
	width, height = font.size

	assert height == font_height and width % font_width == 0
	final_data = []

	for i in range(width // font_width):
		data = [0]
		bits_written = 0
		startx = i * font_width
		for dx in range(font_width):
			for dy in range(font_height):
				pixel = font.getpixel((startx + dx, dy))
				assert type(pixel) == int

				pixel = min(pixel, 1)
				data[-1] |= (pixel << (7 - bits_written))

				bits_written += 1
				if bits_written >= 8:
					data.append(0)
					bits_written = 0

		if bits_written == 0:
			data = data[:-1]

		final_data += data

	with open(os.path.join('.', 'font.hex'), 'wb') as f:
		f.write(bytes(final_data))

if __name__ == '__main__':
	main()
