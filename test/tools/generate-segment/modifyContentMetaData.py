import struct
import sys
import argparse
import shutil
import os

def modify_base_media_decode_time(filename, new_time, scale):
    current_time = None
    box_cnt = 0
    for f, size in BoxFinder(filename,b'tfdt'):
        version_flags = f.read(4)
        version = version_flags[0]
        """
        When generating a non-chunked segment then there is only one moof and
        base media time. We want to ignore the value that ffmpeg has put in
        and just write in a new value as given on the command line.

        When generating chunked segments then there are multiple moof's and the
        base media time of the first moof is assumed to be 0. Subsequent moof's
        will have a base media time increased depending on the size of the previous
        chunk. There may also be a change in time scale. In this case
        new_base_media_time = current_time + new_time
        should give the correct updated decode time

        """
        if version == 0:
            current_time = struct.unpack('>I', f.read(4))[0]
            f.seek(-4, 1)
            if box_cnt==0:
                updated_time = int(new_time)
            else:
                updated_time = int(new_time+(current_time*scale))
            f.write(struct.pack('>I', ))
            print(f"updated baseMediaDecodeTime scale {scale} {current_time} -> {updated_time} (32-bit)")
        elif version == 1:
            current_time = struct.unpack('>Q', f.read(8))[0]
            f.seek(-8, 1)
            if box_cnt==0:
                updated_time = int(new_time)
            else:
                updated_time = int(new_time+(current_time*scale))
            f.write(struct.pack('>Q', updated_time))
            print(f"updated baseMediaDecodeTime scale {scale} {current_time} -> {updated_time} (64-bit)")
            f.flush()
        box_cnt+=1

    if current_time is None:
        print("Could not find the required boxes under moof/traf/tfdt to update baseMediaDecodeTime.", file=sys.stderr)
        exit(1)

def modify_timescale_mvhd(filename, new_timescale):
    with open(filename, 'rb+') as f:
        while True:
            header = f.read(8)
            if len(header) < 8:
                break
            size, box_type = struct.unpack('>I4s', header)
            if box_type == b'moov':
                moov_size = size - 8
                while moov_size > 0:
                    header = f.read(8)
                    if len(header) < 8:
                        break
                    inner_size, inner_box_type = struct.unpack('>I4s', header)
                    if inner_box_type == b'mvhd':
                        version_flags = f.read(4)
                        version = version_flags[0]
                        if version == 0:
                            f.seek(8, 1)
                        elif version == 1:
                            f.seek(16, 1)
                        else:
                            return
                        current_timescale = struct.unpack('>I', f.read(4))[0]
                        f.seek(-4, 1)
                        f.write(struct.pack('>I', new_timescale))
                        print(f"updated timescale {current_timescale} -> {new_timescale}")
                        f.flush()
                        return
                    else:
                        f.seek(inner_size - 8, 1)
                    moov_size -= inner_size
            else:
                f.seek(size - 8, 1)

def modify_timescale_mdhd(filename, new_timescale):
    with open(filename, 'rb+') as f:
        while True:
            header = f.read(8)
            if len(header) < 8:
                break
            size, box_type = struct.unpack('>I4s', header)
            if box_type == b'moov':
                moov_size = size - 8
                while moov_size > 0:
                    header = f.read(8)
                    if len(header) < 8:
                        break
                    trak_size, trak_box_type = struct.unpack('>I4s', header)
                    if trak_box_type == b'trak':
                        trak_remaining = trak_size - 8
                        while trak_remaining > 0:
                            header = f.read(8)
                            if len(header) < 8:
                                break
                            mdia_size, mdia_box_type = struct.unpack('>I4s', header)
                            if mdia_box_type == b'mdia':
                                found_mdia = True
                                mdia_remaining = mdia_size - 8
                                while mdia_remaining > 0:
                                    header = f.read(8)
                                    if len(header) < 8:
                                        break
                                    mdhd_size, mdhd_box_type = struct.unpack('>I4s', header)
                                    if mdhd_box_type == b'mdhd':
                                        found_mdhd = True
                                        version_flags = f.read(4)
                                        version = version_flags[0]
                                        if version == 0:
                                            f.seek(8, 1)
                                        elif version == 1:
                                            f.seek(16, 1)
                                        else:
                                            return
                                        current_timescale = struct.unpack('>I', f.read(4))[0]
                                        f.seek(-4, 1)
                                        f.write(struct.pack('>I', new_timescale))
                                        print(f"updated mdhd timescale {current_timescale} -> {new_timescale}")
                                        f.flush()
                                        return
                                    else:
                                        f.seek(mdhd_size - 8, 1)
                                    mdia_remaining -= mdhd_size
                            else:
                                f.seek(mdia_size - 8, 1)
                            trak_remaining -= mdia_size
                    else:
                        f.seek(trak_size - 8, 1)
                    moov_size -= trak_size
            else:
                # Skip other boxes
                f.seek(size - 8, 1)

def remove_sidx_box_in_place(filename):
    temp_filename = filename + ".temp"

    with open(filename, 'rb') as f_in, open(temp_filename, 'wb') as f_out:
        while True:
            header = f_in.read(8)
            if len(header) < 8:
                # print("End of file.")
                break

            # Unpack size and box type
            size, box_type = struct.unpack('>I4s', header)

            if box_type == b'sidx':
                # print("Found sidx box, skipping it.")
                # Skip the whole sidx box by seeking forward
                f_in.seek(size - 8, 1)
            else:
                # Copy the current box (header + contents)
                f_out.write(header)
                f_out.write(f_in.read(size - 8))

    # Replace the original file with the temp file
    shutil.move(temp_filename, filename)
    print(f"sidx box removed from {filename}")

def modify_segmentNumber(filename, new_sequence_number):
    with open(filename, 'rb+') as f:
        while True:
            # Read the box header (size and type)
            header = f.read(8)
            if len(header) < 8:
                break
            size, box_type = struct.unpack('>I4s', header)
            if box_type == b'moof':
                # Process the contents of the moof box
                while size > 8:
                    header = f.read(8)
                    if len(header) < 8:
                        # print("End of moof.")
                        break
                    inner_size, inner_box_type = struct.unpack('>I4s', header)
                    if inner_box_type == b'mfhd':
                        version_flags = f.read(4)  # Skip the version and flags
                        current_sequence_number = struct.unpack('>I', f.read(4))[0]  # Read current sequence number
                        # Modify the sequence number
                        f.seek(-4, 1)  # Move back 4 bytes to overwrite the sequence number
                        f.write(struct.pack('>I', new_sequence_number))  # Write the new sequence number
                        print(f"updated sequence number {current_sequence_number} -> {new_sequence_number}")
                        f.flush()
                        return
                    else:
                        # Skip over the current inner box
                        f.seek(inner_size - 8, 1)
                    size -= inner_size
            else:
                # Skip over the current top-level box
                f.seek(size - 8, 1)

def modify_default_sample_duration(filename, scale):

        for f, size in BoxFinder(filename,b'tfhd'):
            # Read flags to check which fields are present
            version_flags = f.read(4)
            flags = struct.unpack('>I', version_flags)[0]

            f.read(4)  # Skip track_ID

            offset = 0  # Offset from track_ID to the field we want to modify

            # Check and adjust offset based on flags
            if flags & 0x000008:  # default_sample_duration
                offset += 4
            if flags & 0x000010:  # default_sample_size
                offset += 4
            if flags & 0x000020:  # default_sample_flags
                offset += 4

            if flags & 0x000008:  # Modify if default_sample_duration is present
                f.seek(-size + 16 + offset, 1)  # Move to default_sample_duration position
                current_duration = struct.unpack('>I', f.read(4))[0]
                new_duration = int(current_duration * scale)
                f.seek(-4, 1)  # Move back 4 bytes to overwrite
                f.write(struct.pack('>I', new_duration))
                print(f"updated default sample duration scale {scale} {current_duration} -> {new_duration}")
                f.flush()
            else:
                print("default_sample_duration not present in tfhd")
                exit(1)

class BoxFinder:
    """
    Iterates through isobmff file and find all boxes of the requested type.
    """
    def __init__(self, filename,box_to_find):
        self.filename = filename
        self.box_to_find = box_to_find

    def __iter__(self):
        self.f = open(self.filename, 'rb+')
        s = os.path.getsize(self.filename)
        self.box_list= [ (0,s) ]
        return self

    def __next__(self):
        #print("__next__ ",self.box_list)
        while len(self.box_list):
            next_box, size = self.box_list.pop()
            self.f.seek(next_box,0)
            while size>8:
                # Read the box header (size and type)
                header = self.f.read(8)
                if len(header) < 8:
                    self.f.close()
                    raise StopIteration
                box_size, box_type = struct.unpack('>I4s', header)
                #print(box_type)
                next_box = self.f.tell() + box_size-8
                remaining_size = size - box_size

                if box_type == self.box_to_find:
                    # Save the position in the file we need
                    # to continue search from 
                    if remaining_size>8:
                        self.box_list.append((next_box, remaining_size))
                    #Return position and size of box we have found
                    return (self.f,box_size)
                elif box_type in [b'moof', b'traf']:
                    # Found a box that we need to dive into
                    # Save position of next box at this level
                    # to return to once we have finished depth search
                    self.box_list.append((next_box, remaining_size))
                    size = box_size
                else:
                    self.f.seek(box_size - 8, 1)
                    size -= box_size
        #Not found
        self.f.close()
        raise StopIteration

parser = argparse.ArgumentParser(description="Modify content metadata")
parser.add_argument("filename", type=str, help="The filename to modify")
parser.add_argument("--baseMediaDecodeTime", type=str, help="base Media Decode Time under sidx box")
parser.add_argument("--timescale", type=str, help="time scale under mvhd or sidx box")
parser.add_argument("--segmentNumber", type=str, help="sequencq number under moof/mfhd")
parser.add_argument("--audioSamplingRate", type=str, help="audioSamplingRate used to correct tfhd box")
args = parser.parse_args()
print ('argument list', sys.argv)

scale = 1
if args.baseMediaDecodeTime and args.audioSamplingRate:
    scale = int(args.timescale)/int(args.audioSamplingRate)

if args.baseMediaDecodeTime:
    modify_base_media_decode_time(args.filename, int(args.baseMediaDecodeTime), scale)

if args.segmentNumber:
    modify_segmentNumber(args.filename, int(args.segmentNumber))

if args.timescale:
    modify_timescale_mvhd(args.filename, int(args.timescale))
    modify_timescale_mdhd(args.filename, int(args.timescale))

    #Note: this does NOT yet update sample_duration list from trun box, which is present when all samples can't be constructed with a common duration
    if args.audioSamplingRate:
        modify_default_sample_duration(args.filename, scale)

#Remove sidx box
remove_sidx_box_in_place(args.filename)
