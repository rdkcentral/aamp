import struct
import sys
import argparse

def modify_base_media_decode_time(filename, new_time):
    with open(filename, 'rb+') as f:
        while True:
            
            header = f.read(8)
            if len(header) < 8:
                print("End of file or incomplete header.")
                break
            size, box_type = struct.unpack('>I4s', header)
            
            if box_type == b'moof':
                print("Found moof box")
                
                while size > 8:
                    
                    header = f.read(8)
                    if len(header) < 8:
                        print("End of moof or incomplete header.")
                        break
                    inner_size, inner_box_type = struct.unpack('>I4s', header)
                    print(f"  Found inner box: {inner_box_type.decode('utf-8')} with size: {inner_size}")
                    if inner_box_type == b'traf':
                        print("Found traf box")
                        
                        while inner_size > 8:
                            header = f.read(8)
                            if len(header) < 8:
                                print("End of traf or incomplete header.")
                                break
                            traf_inner_size, traf_inner_box_type = struct.unpack('>I4s', header)
                            print(f"    Found inner traf box: {traf_inner_box_type.decode('utf-8')} with size: {traf_inner_size}")
                            if traf_inner_box_type == b'tfhd':
                                
                                f.seek(traf_inner_size - 8, 1)
                            elif traf_inner_box_type == b'tfdt':
                                print("Found tfdt box")
                                
                                version_flags = f.read(4)
                                version = version_flags[0]
                                if version == 0:
                                    
                                    current_time = struct.unpack('>I', f.read(4))[0]
                                    print(f"Current baseMediaDecodeTime (32-bit): {current_time}")
                                    f.seek(-4, 1)
                                    f.write(struct.pack('>I', new_time))
                                    print(f"Updated baseMediaDecodeTime to: {new_time} (32-bit)")
                                elif version == 1:
                                    
                                    current_time = struct.unpack('>Q', f.read(8))[0]
                                    print(f"Current baseMediaDecodeTime (64-bit): {current_time}")
                                    
                                    f.seek(-8, 1)
                                    f.write(struct.pack('>Q', new_time))
                                    print(f"Updated baseMediaDecodeTime to: {new_time} (64-bit)")
                                f.flush()
                                break
                            else:
                
                                f.seek(traf_inner_size - 8, 1)
                            inner_size -= traf_inner_size
                    else:
                        
                        f.seek(inner_size - 8, 1)
                    size -= inner_size
            else:
                
                f.seek(size - 8, 1)
                print(f"Skipping box {box_type.decode('utf-8')}")
def modify_mvhd_timescale(filename, new_timescale):
    updated=False
    with open(filename, 'rb+') as f:
        while True:
            header = f.read(8)
            if len(header) < 8:
                print("End of file or incomplete header.")
                break
            size, box_type = struct.unpack('>I4s', header)
            
            if box_type == b'moov':
                print("Found moov box")
                
                moov_size = size - 8
                while moov_size > 0:
                    header = f.read(8)
                    if len(header) < 8:
                        print("End of moov or incomplete header.")
                        break
                    inner_size, inner_box_type = struct.unpack('>I4s', header)
                    print(f"  Found inner box: {inner_box_type.decode('utf-8')} with size: {inner_size}")
                    
                    if inner_box_type == b'mvhd':
                        print("Found mvhd box")
                        
                        version_flags = f.read(4)
                        version = version_flags[0]
                        
                        if version == 0:
                            
                            f.seek(8, 1)
                            current_timescale = struct.unpack('>I', f.read(4))[0]
                            print(f"Current timescale: {current_timescale}")
                            f.seek(-4, 1)
                            f.write(struct.pack('>I', new_timescale))
                            print(f"Updated timescale to: {new_timescale}")
                            updated=True
                        elif version == 1:
                            
                            f.seek(16, 1)
                            current_timescale = struct.unpack('>I', f.read(4))[0]
                            print(f"Current timescale: {current_timescale}")
                            f.seek(-4, 1)
                            f.write(struct.pack('>I', new_timescale))
                            print(f"Updated timescale to: {new_timescale}")
                            updated=True
                        
                        f.flush()
                        break
                    
                    else:
                        f.seek(inner_size - 8, 1)
                    moov_size -= inner_size
            if updated:
                break
            else:
                f.seek(size - 8, 1)
                print(f"Skipping box {box_type.decode('utf-8')}")
def modify_segmentNumber(filename, new_sequence_number):
    with open(filename, 'rb+') as f:
        while True:
            # Read the box header (size and type)
            header = f.read(8)
            if len(header) < 8:
                print("End of file or incomplete header.")
                break
            size, box_type = struct.unpack('>I4s', header)
            
            if box_type == b'moof':
                print("Found moof box")
                
                # Process the contents of the moof box
                while size > 8:
                    header = f.read(8)
                    if len(header) < 8:
                        print("End of moof or incomplete header.")
                        break
                    inner_size, inner_box_type = struct.unpack('>I4s', header)
                    print(f"  Found inner box: {inner_box_type.decode('utf-8')} with size: {inner_size}")
                    
                    if inner_box_type == b'mfhd':
                        print("Found mfhd box")
                        
                        version_flags = f.read(4)  # Skip the version and flags
                        current_sequence_number = struct.unpack('>I', f.read(4))[0]  # Read current sequence number
                        print(f"Current sequence number: {current_sequence_number}")
                        
                        # Modify the sequence number
                        f.seek(-4, 1)  # Move back 4 bytes to overwrite the sequence number
                        f.write(struct.pack('>I', new_sequence_number))  # Write the new sequence number
                        print(f"Updated sequence number to: {new_sequence_number}")
                        f.flush()
                        break
                    else:
                        # Skip over the current inner box
                        f.seek(inner_size - 8, 1)
                    
                    size -= inner_size
            else:
                # Skip over the current top-level box
                f.seek(size - 8, 1)
                print(f"Skipping box {box_type.decode('utf-8')}")
parser = argparse.ArgumentParser(description="Modify content metadata")
parser.add_argument("filename", type=str, help="The filename to modify")
parser.add_argument("--baseMediaDecodeTime", type=str, help="base Media Decode Time under sidx box")
parser.add_argument("--timescale", type=str, help="time scale under mvhd box")
parser.add_argument("--segmentNumber", type=str, help="sequencq number under moof/mfhd")
args = parser.parse_args()
print ('argument list', sys.argv)
if args.baseMediaDecodeTime:
    modify_base_media_decode_time(args.filename, int(args.baseMediaDecodeTime))
if args.timescale:
    modify_mvhd_timescale(args.filename, int(args.timescale))
if args.segmentNumber:
    modify_segmentNumber(args.filename, int(args.segmentNumber))

